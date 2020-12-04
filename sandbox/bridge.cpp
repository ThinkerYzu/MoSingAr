/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "bridge.h"
#include "scout.h"
#include "tinypack.h"

#include "msghelper.h"

#include "log.h"

#include <sys/socket.h>
#include <asm/unistd.h>

#include <memory>
#include <assert.h>


extern "C" {
extern long (*td__syscall_trampo)(long, ...);
}

#define SYSCALL td__syscall_trampo

int sandbox_bridge::send_open(const char* path, int flags, mode_t mode) {
  auto pack = tinypacker()
    .field(scout::cmd_open)
    .field(path)
    .field(flags)
    .field(mode);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

int sandbox_bridge::send_openat(int dirfd, const char* path, int flags, mode_t mode) {
  LOGU(send_openat);
  auto pack = tinypacker()
    .field(scout::cmd_openat)
    .field(dirfd)
    .field(path)
    .field(flags)
    .field(mode);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix(), dirfd);
  free(buf);

  rcvr->receive_one();
  assert(rcvr->get_data_bytes() == 2 * sizeof(int));
  auto ptr = rcvr->get_data();
  auto ptr_end = ptr + rcvr->get_data_bytes();
  assert((int)(*(int*)ptr + sizeof(int)) == rcvr->get_data_bytes());
  ptr += sizeof(int);

  int r;
  auto unpacker = tinyunpacker(ptr, ptr_end - ptr)
    .field(r);
  assert(unpacker.check_completed());
  unpacker.unpack();
  if (r >= 0) {
    assert(rcvr->get_fd_rcvd_num() == 1);
    r = rcvr->get_fd_rcvd()[0];
  }

  return r;
}

int sandbox_bridge::send_dup(int oldfd) {
  auto r = SYSCALL(__NR_dup, oldfd);
  return r;
}

int sandbox_bridge::send_dup2(int oldfd, int newfd) {
  auto r = SYSCALL(__NR_dup2, oldfd, newfd);
  return r;
}

int sandbox_bridge::send_access(const char* path, int mode) {
  auto pack = tinypacker()
    .field(scout::cmd_access)
    .field(path)
    .field(mode);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix());
  free(buf);

  // Receive reply
  auto ok = rcvr->receive_one();
  if (!ok) {
    return -1;
  }
  assert(rcvr->get_data_bytes() == sizeof(int) * 2);
  auto ptr = rcvr->get_data();
  assert(*(int*)ptr == sizeof(int));
  ptr += sizeof(int);

  int r;
  auto unpacker = tinyunpacker(ptr, sizeof(int))
    .field(r);

  assert(unpacker.check_completed());
  unpacker.unpack();

  return r;
}

int sandbox_bridge::send_fstat(int fd, struct stat* statbuf) {
  auto pack = tinypacker()
    .field(scout::cmd_fstat)
    .field(fd)
    .field(statbuf);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

int sandbox_bridge::send_stat(const char* path, struct stat* statbuf) {
  auto pack = tinypacker()
    .field(scout::cmd_stat)
    .field(path)
    .field(statbuf);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

int sandbox_bridge::send_lstat(const char* path, struct stat* statbuf) {
  auto pack = tinypacker()
    .field(scout::cmd_lstat)
    .field(path)
    .field(statbuf);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

/**
 * The bridge handles |execve()| by executing a trampoline program,
 * that in turn will execute the program that the caller want to
 * execute after setting |LD_PRELOAD| environment variable properly to
 * make sure libtongdao.so being loaded and setting a sandbox.
 *
 * The filter will trap all |execve()| calls except ones running the
 * trampoline program.
 */
int sandbox_bridge::send_execve(const char* filename, char*const* argv, char*const* envp) {
  LOGU(sandbox_bridge::send_execve);
  auto pid = SYSCALL(__NR_getpid);
  if (pid < 0) {
    return pid;
  }
  auto pack = tinypacker()
    .field(scout::cmd_execve)
    .field((int)pid)
    .field(filename);
  auto buf = pack.pack_size_prefix();
  LOGU(send_msg);
  send_msg(sock, buf, pack.get_size_prefix());
  auto ok = rcvr->receive_one();
  if (!ok) {
    LOGU(!ok);
    return -1;
  }

  return 0;
  auto r = SYSCALL(__NR_execve, filename, argv, envp);
  return r;
}

size_t sandbox_bridge::send_readlink(const char* path, char* _buf, size_t _bufsize) {
  auto pack = tinypacker()
    .field(scout::cmd_readlink)
    .field(path);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

int sandbox_bridge::send_unlink(const char* path) {
  auto pack = tinypacker()
    .field(scout::cmd_unlink)
    .field(path);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

pid_t sandbox_bridge::send_vfork() {
  auto pack = tinypacker()
    .field(scout::cmd_vfork);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

void sandbox_bridge::set_sock(int fd) {
  sock = fd;
  // This instance is never deleted.
  auto buf = malloc(sizeof(msg_receiver));
  rcvr = new(buf) msg_receiver(fd);
}

int
sandbox_bridge::send_rt_sigaction(int signum,
                                  const struct sigaction* act,
                                  struct sigaction* oldact,
                                  size_t sigsetsz) {
  if (signum == SIGSYS && act != nullptr) {
    // Just skip it for now.
    // We need a better implementation to call user's handler.
    return 0;
  }
  return SYSCALL(__NR_rt_sigaction,
                 (long)signum,
                 (long)act,
                 (long)oldact,
                 (long)sigsetsz);
}
