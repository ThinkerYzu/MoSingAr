/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "bridge.h"
#include "scout.h"
#include "tinypack.h"

#include "msghelper.h"

#include "log.h"

#include <unistd.h>
#include <sys/socket.h>
#include <asm/unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <memory>
#include <assert.h>


extern "C" {
extern long (*td__syscall_trampo)(long, ...);
}

#define SYSCALL td__syscall_trampo

static int
receive_int_msg(msg_receiver* rcvr) {
  auto ok = rcvr->receive_one();
  assert(ok);
  auto ptr = rcvr->get_data();
  assert(rcvr->get_data_bytes() == 2 * sizeof(int));
  assert(*(int*)ptr == sizeof(int));
  assert((int)(*(int*)ptr + sizeof(int)) == rcvr->get_data_bytes());

  ptr += sizeof(int);
  auto r = *(int*)ptr;
  return r;
}

template<typename T>
static int
receive_struct_msg(msg_receiver* rcvr, T* buf) {
  auto ok = rcvr->receive_one();
  assert(ok);
  unsigned int payload_sz;
  int r;
  auto unpacker = tinyunpacker(rcvr->get_data(), rcvr->get_data_bytes())
    .field(payload_sz)
    .field(r)
    .field(*buf);
  assert(unpacker.check_completed());
  assert(unpacker.get_size() == rcvr->get_data_bytes());

  unpacker.unpack();
  assert((int)(payload_sz + sizeof(int)) == rcvr->get_data_bytes());

  return r;
}

int sandbox_bridge::send_open(const char* path, int flags, mode_t mode) {
  LOGU(send_open);
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
  LOGU(send_dup);
  auto r = SYSCALL(__NR_dup, oldfd);
  return r;
}

int sandbox_bridge::send_dup2(int oldfd, int newfd) {
  LOGU(send_dup2);
  auto r = SYSCALL(__NR_dup2, oldfd, newfd);
  return r;
}

int sandbox_bridge::send_access(const char* path, int mode) {
  LOGU(send_access);
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
  LOGU(send_fstat);
  auto pack = tinypacker()
    .field(scout::cmd_fstat)
    .field(fd);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix(), fd);
  free(buf);

  auto r = receive_struct_msg(rcvr, statbuf);

  return r;
}

int sandbox_bridge::send_stat(const char* path, struct stat* statbuf) {
  LOGU(send_stat);
  auto pack = tinypacker()
    .field(scout::cmd_stat)
    .field(path);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix());
  free(buf);

  auto r = receive_struct_msg(rcvr, statbuf);

  return r;
}

int sandbox_bridge::send_lstat(const char* path, struct stat* statbuf) {
  LOGU(send_lstat);
  auto pack = tinypacker()
    .field(scout::cmd_lstat)
    .field(path);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix());
  free(buf);

  auto r = receive_struct_msg(rcvr, statbuf);

  return r;
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
  send_msg(sock, buf, pack.get_size_prefix());
  auto ok = rcvr->receive_one();
  if (!ok) {
    LOGU(!ok);
    return -1;
  }

  return 0;
}

extern "C" {
extern void printptr(void* p);
}

size_t sandbox_bridge::send_readlink(const char* path, char* buf, size_t bufsize) {
  LOGU(send_readlink);
  auto pack = tinypacker()
    .field(scout::cmd_readlink)
    .field(path)
    .field(bufsize);
  auto _buf = pack.pack_size_prefix();
  send_msg(sock, _buf, pack.get_size_prefix());
  free(_buf);

  auto ok = rcvr->receive_one();
  if (!ok) {
    LOGU(!ok);
    return -1;
  }

  auto ptr = rcvr->get_data();
  unsigned int payload_sz;
  int retv;
  fixedbuf fbuf(buf, bufsize);
  auto unpacker = tinyunpacker(ptr, rcvr->get_data_bytes())
    .field(payload_sz)
    .field(retv)
    .field(fbuf);
  assert(unpacker.check_completed());

  unpacker.unpack();
  assert((unsigned int)unpacker.get_size() == (unsigned int)rcvr->get_data_bytes());
  assert(payload_sz + sizeof(unsigned int) == (unsigned int)rcvr->get_data_bytes());

  return retv;
}

int sandbox_bridge::send_unlink(const char* path) {
  LOGU(send_unlink);
  auto pack = tinypacker()
    .field(scout::cmd_unlink)
    .field(path);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix());
  free(buf);

  auto r = receive_int_msg(rcvr);
  return r;
}

pid_t sandbox_bridge::send_vfork() {
  LOGU(send_vfork);
  auto pid = (pid_t)SYSCALL(__NR_getpid);
  auto pack = tinypacker()
    .field(scout::cmd_vfork)
    .field(pid);
  auto buf = pack.pack_size_prefix();
  send_msg(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

int
sandbox_bridge::send_rt_sigaction(int signum,
                                  const struct sigaction* act,
                                  struct sigaction* oldact,
                                  size_t sigsetsz) {
  LOGU(send_rt_sigaction);
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

void sandbox_bridge::set_sock(int fd) {
  sock = fd;
  // This instance is never deleted.
  auto buf = malloc(sizeof(msg_receiver));
  rcvr = new(buf) msg_receiver(fd);
}
