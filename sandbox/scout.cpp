/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "scout.h"
#include "cmdcenter.h"
#include "bridge.h"
#include "tinypack.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mman.h>

#include <string.h>

#define assert(x) do { if (!(x)) { abort(); } } while(0)


extern "C" {
extern long syscall_trampoline(long, ...);
// The syscall trampoline, it will point to a copy at a fixed address
// later that the seccomp filter will always allow it.
long (*td__syscall_trampo)(long, ...) = syscall_trampoline;

extern unsigned long int global_flags;
}

scout::~scout() {
  if (sock != -1) {
    close(sock);
  }
}

#if !defined(DUMMY) || defined(TEST_CC_CHANNEL)

bool
scout::establish_cc_channel() {
  int socks[2];
  auto r = socketpair(AF_UNIX, SOCK_DGRAM, 0, socks);
  if (r < 0) {
    perror("socketpair");
    return false;
  }

  int cmd = cmdcenter::SCOUT_CONNECT_CMD;
  iovec iov = {
    .iov_base = &cmd,
    .iov_len = sizeof(cmd)
  };
  char cmsg_buf[CMSG_SPACE(sizeof(int))];
  msghdr msg = { 0 };
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);
  auto cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  *(int*)CMSG_DATA(cmsg) = socks[1];
  r = sendmsg(CARRIER_SOCK, &msg, 0);
  if (r < 0) {
    perror("sendmsg");
    close(socks[0]);
    close(socks[1]);
    return false;
  }

  close(socks[1]);
  sock = socks[0];
  fcntl(sock, F_SETFD, FD_CLOEXEC);

  return true;
}

#endif  // !DUMMY || TEST_CC_CHANNEL

#if !defined(DUMMY)

bool
scout::init_sandbox() {
  if (!(global_flags & FLAG_CC_COMM_READY)) {
    establish_cc_channel();
  }

  auto sigsys_r = install_sigsys();
  assert(sigsys_r);
  if (!(global_flags & FLAG_FILTER_INSTALLED)) {
    auto filter_r = install_seccomp_filter();
    assert(filter_r);
  }
  return true;
}

bool
scout::install_syscall_trampo() {
  // Setup the trampoline, the filter will always allow trampoline's
  // request.
  void* ptr = mmap((void*)TRAMPOLINE_ADDR,
                   4096,
                   PROT_READ | PROT_WRITE | PROT_EXEC,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                   -1,
                   0);
  assert(ptr != nullptr);
  assert(ptr == (void*)TRAMPOLINE_ADDR);
  memcpy(ptr, (void*)syscall_trampoline, 4096);
  td__syscall_trampo = (long(*)(long, ...))ptr;
  return true;
}

extern void install_seccomp_sigsys(int ccsock);
extern void install_seccomp_filter();

bool
scout::install_sigsys() {
  install_seccomp_sigsys(sock);
  return true;
}

bool
scout::install_seccomp_filter() {
  ::install_seccomp_filter();
  return true;
}

#endif  // !DUMMY
