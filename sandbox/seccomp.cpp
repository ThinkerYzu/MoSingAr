/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "seccomp.h"

#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <asm/unistd.h>

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>

static int seccomp(unsigned int operation, unsigned int flags, void *args) {
  return syscall(__NR_seccomp, operation, flags, args);
}

static int
install_filter() {
  extern struct sock_fprog sandbox_filter_prog;

  if (seccomp(SECCOMP_SET_MODE_FILTER, 0, &sandbox_filter_prog)) {
    perror("seccomp");
    return 1;
  }
  return 0;
}

static void
handle_syscall(siginfo_t* info, ucontext_t* context) {
  auto ctx = context;
  SECCOMP_RESULT(ctx) = 111;
}

static void
sigsys(int nr, siginfo_t *info, void* void_context) {
  printf("sigsys %d\n", nr);
  ucontext_t *ctx = (ucontext_t*)void_context;
  handle_syscall(info, ctx);
}

static void
install_sigsys() {
  struct sigaction act;
  act.sa_sigaction = &sigsys;
  act.sa_flags = SA_SIGINFO | SA_NODEFER;
  int r = sigaction(SIGSYS, &act, NULL);
  if (r < 0) {
    perror("sigaction");
  }
}

/**
 * The BPF filter will check all syscalls, some of calls are allowed
 * by return SECCOMP_RET_ALLOW. Other calls are trapped by return
 * SECCOMP_RET_TRAP.
 *
 * By returning SECCOMP_RET_TRAP from the filter, it triggers SIGSYS
 * at the thread calling the syscall.  In the signal handler of
 * SIGSYS, it handle the syscall by changing registers or memory with
 * the information coming along with siginfo_t and ucontext.
 */
int
init_seccomp() {
  install_sigsys();
  install_filter();
}
