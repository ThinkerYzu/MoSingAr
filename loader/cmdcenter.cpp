/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "cmdcenter.h"
#include "flightdeck.h"
#include "scout.h"
#include "ptracetools.h"
#include "tinypack.h"
#include "msghelper.h"

#include "log.h"
#include "errhandle.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <asm/unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <assert.h>
#include <errno.h>

#include <memory>
#include <algorithm>


bool sigchld_ignore = false;

template<typename T>
int
send_msg_packer(int sock, T& packer, int fd1 = -1, int fd2 = -1) {
  auto reply = packer.pack_size_prefix();
  int r = send_msg(sock, reply, packer.get_size_prefix(), fd1, fd2);

  free(reply);
  return r;
}

cmdcenter::cmdcenter(int fd)
  : stopping_message(false)
  , efd(-1)
  , carrierfd(fd) {}

cmdcenter::~cmdcenter() {
  for (auto itr = scoutfds.begin();
       itr != scoutfds.end();
       ++itr) {
    close(*itr);
  }
  close(efd);
}

bool
cmdcenter::init() {
  efd = epoll_create1(EPOLL_CLOEXEC);
  if (efd < 0) {
    perror("epoll_create1");
    return false;
  }

  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = carrierfd;
  auto r = epoll_ctl(efd, EPOLL_CTL_ADD, carrierfd, &ev);
  if (r < 0) {
    perror("epoll_ctl");
    return false;
  }

  return true;
}

bool
cmdcenter::handle_message() {
  epoll_event events[max_events];

 retry_intr:
  int num = epoll_wait(efd, events, max_events, -1);
  if (num < 0) {
    if (errno == EINTR) {
      if (stopping_message) {
        return false;
      }
      goto retry_intr;
    }
    perror("epoll_wait");
    return false;
  }

  for (auto i = 0; i < num; i++) {
    auto ev = events + i;
    if (ev->events & EPOLLRDHUP) {
      remove_scout(ev->data.fd);
    } else if (ev->events & EPOLLIN) {
      auto sock = ev->data.fd;
      if (sock == carrierfd) {
        auto r = handle_carrier_msg();
        if (!r) {
          return r;
        }
      } else {
        handle_scout_msg(sock);
      }
    }
  }

  return true;
}

void
cmdcenter::handle_messages() {
  while (!stopping_message && handle_message()) {
  }
  stopping_message = false;
}

bool
cmdcenter::handle_exec(pid_t pid, int sock) {
  LOGU(handle_exec);

  // Attach & trace the process
  _E(ptrace_attach, pid);
  ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACEEXEC);
  // Make the process running
  ptrace_cont(pid);
  int ok = 1;
  auto packer = tinypacker()
    .field(ok);
  auto reply = packer.pack_size_prefix();
  _E(send_msg, sock, reply, packer.get_size_prefix());
  // Wait for exec()
  auto evt = ptrace_waittrap(pid);
  if (evt < 0) {
    return false;
  }
  if (evt == PTRACE_EVENT_EXEC) {
    // Run the first instruction of the tracee before code injection.
    //
    // This is required to set the values of registers correctly for the
    // injected code.  Without this, PTRACE_SETREGS will take no
    // effects.  I guess it is overwrote by the kernel since kernel
    // haven't returned to the user space of the process, and it may set
    // the registers with values when it returns to the user space first
    // time.
    _E(ptrace_stepi, pid);

    // Install the signal handler and establish a channel, but not
    // install the seccomp filter.
    _E(flightdeck::scout_takeoff, pid, scout::FLAG_FILTER_INSTALLED);
  } else {
    // execve() fails! Stop tracing.
    //
    // This SIGTRAP was made up by the scout to notify the Command
    // Center for only the success execve() making SIGTRAP, failed one
    // doesn't make SIGTRAP.
    assert(evt == 0);
  }

  _E(ptrace, PTRACE_SETOPTIONS, pid, 0, 0);

  // Enable SIGCHLD handler before detaching in case of any lost
  // signals.
  sigchld_ignore = false;

  auto r = ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
  if (r < 0) {
    perror("ptrace PTRACE_DETACH");
    return false;
  }

  return true;
}

bool
cmdcenter::add_scout(int scoutfd) {
  assert(find(scoutfds.begin(), scoutfds.end(), scoutfd) == scoutfds.end());
  scoutfds.push_back(scoutfd);
  epoll_event ev;
  ev.events = EPOLLIN | EPOLLRDHUP;
  ev.data.fd = scoutfd;
  auto r = epoll_ctl(efd, EPOLL_CTL_ADD, scoutfd, &ev);
  if (r < 0) {
    perror("epoll_ctl");
    return false;
  }
  return true;
}

bool
cmdcenter::remove_scout(int scoutfd) {
  bool success = false;
  scoutfds.remove_if([&](const int& v) {
      if (v == scoutfd) {
        auto r = epoll_ctl(efd, EPOLL_CTL_DEL, scoutfd, nullptr);
        if (r < 0) {
          perror("epoll_ctl");
        }
        close(scoutfd);
        success = true;
      }
      return success;
    });
  return success;
}

void
cmdcenter::stop_msg_loop() {
  int cmd = STOP_MSG_LOOP_CMD;
  send(CARRIER_SOCK, &cmd, sizeof(cmd), 0);
}

/**
 * Start a new mission of running with given arguments.
 *
 * It forks a new process to run the mission and launch a scout.
 */
pid_t
cmdcenter::start_mission(int argc, char*const* argv) {
  int toffsocks[2];
  _EI(socketpair, AF_UNIX, SOCK_STREAM, 0, toffsocks);

  auto childpid = fork();
  if (childpid < 0) {
    perror("fork");
    close(toffsocks[0]);
    close(toffsocks[1]);
    return -1;
  }
  if (childpid) {
    // parent
    _EI(close, toffsocks[1]);

    auto r = ptrace(PTRACE_ATTACH, childpid, nullptr, 0);
    if (r < 0) {
      perror("ptrace");
      return -1;
    }
    // Wait the child to be attached.
    int status;
    do {
      _EA(waitpid, childpid, &status, 0);
    } while(!WIFSTOPPED(status) && WSTOPSIG(status) != SIGTRAP);

    _EI(flightdeck::scout_takeoff, childpid, 0);

    // Detach the process or it might be blocked for serveral reasons.
    r = ptrace(PTRACE_DETACH, childpid, nullptr, 0);
    if (r < 0) {
      perror("ptrace");
      return -1;
    }

    // Has taken off. Let the child continue.
    char buf =  0xff;
    _EI(write, toffsocks[0], &buf, 1);
    _EI(close, toffsocks[0]);
  } else {
    // child
    _EA(close, toffsocks[0]);

    // Wait for taking off in the parent process
    //char buf;
    char buf = 0xff;
    _EA(read, toffsocks[1], &buf, 1);
    _EA(close, toffsocks[1]);

    _EA(execvpe, argv[0], argv, environ);
    // Should not be here!
  }
  return childpid;
}

bool
cmdcenter::handle_carrier_msg() {
  auto rcvr = std::make_unique<msg_receiver>(carrierfd);
  auto ok = rcvr->receive_one();
  if (!ok) {
    return false;
  }

  assert(rcvr->get_data_bytes() == sizeof(int));
  int cmd = *(int*)rcvr->get_data();
  if (cmd == SCOUT_CONNECT_CMD) {
    assert(rcvr->get_fd_rcvd_num() == 1);
    int sock = *rcvr->get_fd_rcvd();
    add_scout(sock);
  } else if (cmd == STOP_MSG_LOOP_CMD) {
    stopping_message = true;
  }

  return true;
}

bool
cmdcenter::handle_scout_msg(int sock) {
  LOGU(handle_scout_msg);
  auto rcvr = std::make_unique<msg_receiver>(sock);
  auto ok = rcvr->receive_one();
  if (!ok) {
    return false;
  }

  auto ptr = rcvr->get_data();
  auto payload_bytes = *(int*)ptr;
  ptr += sizeof(int);
  auto data_end = ptr + payload_bytes;
  assert((unsigned)rcvr->get_data_bytes() == (payload_bytes + sizeof(int)));

  auto cmd = *(int*)ptr;
  ptr += sizeof(int);

  switch (cmd) {
  case scout::cmd_hello:
    assert(ptr == data_end);
    assert(rcvr->get_fd_rcvd_num() == 0);
    break;

  case scout::cmd_openat:
    {
      LOGU(cmd_openat);
      int dirfd;
      const char* path;
      int flags;
      mode_t mode;
      auto unpacker = tinyunpacker(ptr, data_end - ptr)
        .field(dirfd)
        .field(path)
        .field(flags)
        .field(mode);

      assert(unpacker.check_completed());
      unpacker.unpack();

      assert(rcvr->get_fd_rcvd_num() == 1 || dirfd < 0);
      if (dirfd >= 0) {
        dirfd = rcvr->get_fd_rcvd()[0];
      }

      auto fd = openat(dirfd, path, flags, mode);
      if (fd < 0) {
        fd = -errno;
      }
      if (dirfd >= 0) {
        close(dirfd);
      }

      auto packer = tinypacker()
        .field(fd);
      auto reply = packer.pack_size_prefix();
      auto r = send_msg(sock, reply, packer.get_size_prefix(), fd);
      close(fd);
      if (r < 0) {
        return false;
      }

      free((void*)path);
      free(reply);
    }
    break;

  case scout::cmd_access:
    {
      LOGU(cmd_access);
      char* path;
      int mode;
      auto unpacker = tinyunpacker(ptr, data_end -ptr)
        .field(path)
        .field(mode);

      assert(unpacker.check_completed());
      unpacker.unpack();
      LOGU(access);

      auto r = access(path, mode);
      if (r < 0) {
        r = -errno;
      }

      LOGU(packer);
      auto packer = tinypacker()
        .field(r);
      packer.pack();
      auto result = packer.pack_size_prefix();
      send_msg(sock, result, packer.get_size_prefix());

      free(result);
      free(path);
    }
    break;

  case scout::cmd_fstat:
    {
      LOGU(cmd_fstat);
      int fd;
      auto unpacker = tinyunpacker(ptr, data_end - ptr)
        .field(fd);

      assert(unpacker.check_completed());
      unpacker.unpack();
      if (fd >= 0) {
        if (rcvr->get_fd_rcvd_num() != 1) {
          sleep(300);
        }
        assert(rcvr->get_fd_rcvd_num() == 1);
        fd = rcvr->get_fd_rcvd()[0];
      }

      struct stat statbuf;
      auto r = fstat(fd, &statbuf);
      if (r < 0) {
        r = -errno;
      }
      if (fd >= 0) {
        close(fd);
      }

      auto packer = tinypacker()
        .field(r)
        .field(statbuf);
      _E(send_msg_packer, sock, packer);
    }
    break;

  case scout::cmd_stat:
    {
      LOGU(cmd_fstat);
      const char* path;
      auto unpacker = tinyunpacker(ptr, data_end - ptr)
        .field(path);

      assert(unpacker.check_completed());
      unpacker.unpack();

      struct stat statbuf;
      auto r = stat(path, &statbuf);
      if (r < 0) {
        r = -errno;
      }
      free((void*)path);

      auto packer = tinypacker()
        .field(r)
        .field(statbuf);
      _E(send_msg_packer, sock, packer);
    }
    break;

  case scout::cmd_lstat:
    {
      LOGU(cmd_fstat);
      const char* path;
      auto unpacker = tinyunpacker(ptr, data_end - ptr)
        .field(path);

      assert(unpacker.check_completed());
      unpacker.unpack();

      struct stat statbuf;
      auto r = lstat(path, &statbuf);
      if (r < 0) {
        r = -errno;
      }
      free((void*)path);

      auto packer = tinypacker()
        .field(r)
        .field(statbuf);
      _E(send_msg_packer, sock, packer);
    }
    break;

  case scout::cmd_execve:
    {
      LOGU(cmd_execve);
      char* path;
      int pid;
      auto unpacker = tinyunpacker(ptr, data_end - ptr)
        .field(pid)
        .field(path);

      assert(unpacker.check_completed());
      unpacker.unpack();
      free(path);

      extern bool sigchld_ignore;
      sigchld_ignore = true;
      auto ok = handle_exec(pid, sock);
      sigchld_ignore = false;
      if (!ok) {
        return false;
      }

    }
    break;

  case scout::cmd_readlink:
    {
      LOGU(cmd_readlink);
      const char* path;
      size_t bufsize;
      auto unpacker = tinyunpacker(ptr, data_end - ptr)
        .field(path)
        .field(bufsize);

      assert(unpacker.check_completed());
      unpacker.unpack();

      auto buf = new char[bufsize];
      auto retv = (int)readlink(path, buf, bufsize);
      if (retv < 0) {
        retv = -errno;
      }

      fixedbuf fbuf(buf, bufsize);
      auto packer = tinypacker()
        .field(retv)
        .field(fbuf);
      _E(send_msg_packer, sock, packer);

      delete buf;
      free((void*)path);
    }
    break;

  case scout::cmd_unlink:
    {
      LOGU(cmd_unlink);
      const char* path;
      auto unpacker = tinyunpacker(ptr, data_end - ptr)
        .field(path);

      assert(unpacker.check_completed());
      unpacker.unpack();

      auto r = unlink(path);
      if (r < 0) {
        r = -errno;
      }

      auto packer = tinypacker()
        .field(r);
      _E(send_msg_packer, sock, packer);

      free((void*)path);
    }
    break;

  default:
    printf("Unknown cmd %x\n", cmd);
    return false;
  }

  return true;
}
