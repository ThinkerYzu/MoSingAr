/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "cmdcenter.h"
#include "flightdeck.h"
#include "scout.h"
#include "ptracetools.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/ptrace.h>
#include <assert.h>
#include <errno.h>

#include <memory>
#include <algorithm>

#define _E(name, args...)                       \
  do {                                          \
    auto r = name(args);                        \
    if (r < 0) { perror(#name); return false; } \
  } while( 0)
#define _EI(name, args...)                      \
  do {                                          \
    auto r = name(args);                        \
    if (r < 0) { perror(#name); return r; }     \
  } while( 0)
#define _EA(name, args...)                      \
  do {                                          \
    auto r = name(args);                        \
    if (r < 0) { perror(#name); abort(); }      \
  } while( 0)

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

namespace {

/**
 * Receive one message a time from the sock that may along with FDs.
 */
class msg_receiver {
public:
  constexpr static int data_buf_size = 1024;
  // It can handle at most 2 FDs in a message.
  constexpr static int fd_rcvd_size = 2;

  msg_receiver(int fd)
    : fd(fd)
    , fd_rcvd_num(0)
    , data_bytes(0) {
  }

  bool receive_one();

  int get_data_bytes() { return data_bytes; }
  char* get_data() { return data; }

  int get_fd_rcvd_num() { return fd_rcvd_num; }
  int* get_fd_rcvd() { return fd_rcvd; }

private:
  int fd;
  int fd_rcvd_num;
  int fd_rcvd[fd_rcvd_size];
  int data_bytes;
  char data[data_buf_size];
};

bool
msg_receiver::receive_one() {
  auto cmsg_buf_sz = CMSG_SPACE(sizeof(int) * fd_rcvd_size);
  char cmsg_buf[cmsg_buf_sz];
  iovec vec = { data, data_buf_size };
  msghdr msg = { nullptr, 0, &vec, 1, cmsg_buf, cmsg_buf_sz, 0 };

  data_bytes = recvmsg(fd, &msg, MSG_DONTWAIT);
  if (data_bytes < 0) {
    perror("recvmsg");
    return false;
  }
  assert((unsigned)data_bytes >= sizeof(int));
  assert(!(msg.msg_flags & MSG_TRUNC));

  if (msg.msg_controllen >= CMSG_SPACE(sizeof(int))) {
    auto cmsg = CMSG_FIRSTHDR(&msg);
    fd_rcvd_num = cmsg->cmsg_len / CMSG_LEN(sizeof(int));
    assert(fd_rcvd_num <= fd_rcvd_size);
    for (int i = 0; i < fd_rcvd_num; i++) {
      fd_rcvd[i] = ((int*)CMSG_DATA(cmsg))[i];
    }
  }
  return true;
}

} // namespace

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

void
cmdcenter::handle_exec(pid_t pid) {
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

    _EI(flightdeck::scout_takeoff, childpid, 0);

    ptrace_cont(childpid);

    // Has taken off. Let the child continue.
    char buf =  0xff;
    _EI(write, toffsocks[0], &buf, 1);
    _EI(close, toffsocks[0]);
  } else {
    // child
    _EA(close, toffsocks[0]);

    // Wait for taking off in the parent process
    char buf;
    _EA(read, toffsocks[1], &buf, 1);
    _EA(close, toffsocks[1]);

    _EA(execv, argv[0], argv);
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

  default:
    printf("Unknown cmd %x\n", cmd);
    return false;
  }

  return true;
}
