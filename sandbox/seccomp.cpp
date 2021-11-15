/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "seccomp.h"
#include "bridge.h"
#include "scout.h"

#include "log.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <asm/unistd.h>
#include <string.h>

#include <sys/prctl.h>

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>

#include <fcntl.h>
#include <sys/stat.h>

#define assert(x)                  \
  do {                             \
    if (!(x)) {                    \
      write(2, "assertion: ", 11); \
      write(2, #x, strlen(#x));    \
      char lf = '\n';              \
      write(2, &lf, 1);            \
      abort();                     \
    }                              \
  } while(0)


extern "C" {
extern int seccomp(unsigned int, unsigned int, void *);
extern long (*td__syscall_trampo)(long, ...);
extern long (*td__vfork_trampo)();
extern int fakeframe_trampoline();
extern void printptr(void* p);
}

#define SYSCALL td__syscall_trampo
#define VFORK() td__vfork_trampo()

static int
install_filter() {
  extern struct sock_fprog sandbox_filter_prog;

  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
    perror("prctl");
    abort();
  }
  if (seccomp(SECCOMP_SET_MODE_FILTER, 0, &sandbox_filter_prog)) {
    perror("seccomp");
    abort();
  }
  return 0;
}

static sandbox_bridge bridge;

static long
execve_handler(const char *path, char*const* argv, char*const* envp) {
  LOGU(execve_handler);
  auto r = SYSCALL(__NR_execve, (long)path, (long)argv, (long)envp);
  if (r < 0) {
    // Notify the Command Center that the call fails.
    // execve() cause SIGTRAP only if success.
    SYSCALL(__NR_kill, getpid(), SIGTRAP);
  }
  return r;
}

static long
vfork_handler(char *rsp, char *old_rsp) {
  static char buf[256];

  LOGU(vfork_handler);

  auto pid = VFORK();
  if (pid == 0) {
    scout::getInstance()->establish_cc_channel();
    int keep_size = old_rsp - rsp;
    assert(keep_size <= (int)(256 - sizeof(void*) * 2));
    auto src = (void**)rsp;
    auto ptr = (void**)buf;
    *ptr++ = (void*)(long)keep_size;
    *ptr++ = rsp;
    for (int i = 0; i < keep_size; i += sizeof(void*)) {
      *ptr++ = *src++;
    }
  } else if (pid > 0) {
    LOGU(parent);
    auto src = (void**)buf;
    long restore_size = (long)*src++;
    auto ptr = (void**)*src++;
    for (int i = 0; i < restore_size; i += sizeof(void*)) {
      *ptr++ = *src++;
    }
  }
  return pid;
}

static void
install_fakeframe(ucontext_t* ctx, void* user_handler, void* saved_rsp = nullptr) {
  // Call the user handler after leaving the signal handler, and
  // return to the user space code.

  //
  // make a fake frame to call the user handler
  //
  {
    auto rsp = (void**)SECCOMP_REG(ctx, REG_RSP);

    // Return to the caller from the fake frame
    *--rsp = (void*)SECCOMP_IP(ctx);
    // Restore rsp
    *--rsp = saved_rsp ? saved_rsp : (void*)SECCOMP_REG(ctx, REG_RSP);
    // Restore registers
    *--rsp = (void*)SECCOMP_REG(ctx, REG_R11);
    *--rsp = (void*)SECCOMP_REG(ctx, REG_R10);
    *--rsp = (void*)SECCOMP_REG(ctx, REG_R9);
    *--rsp = (void*)SECCOMP_REG(ctx, REG_R8);
    *--rsp = (void*)SECCOMP_REG(ctx, REG_RDX);
    *--rsp = (void*)SECCOMP_REG(ctx, REG_RCX);
    *--rsp = (void*)SECCOMP_REG(ctx, REG_RBX);
    *--rsp = (void*)SECCOMP_REG(ctx, REG_RSI);
    *--rsp = (void*)SECCOMP_REG(ctx, REG_RDI);
    // Return to the fake frame trampoline
    *--rsp = (void*)fakeframe_trampoline;

    // Set the stack pointer
    SECCOMP_REG(ctx, REG_RSP) = (long long unsigned int)rsp;
  }

  // Return to the handler
  SECCOMP_IP(ctx) = (long long unsigned int)user_handler;
}

static void
handle_syscall(siginfo_t* info, ucontext_t* context) {
  LOGU(seccomp handle_syscall);
  auto ctx = context;
  auto syscall = SECCOMP_SYSCALL(ctx);

  switch (syscall) {
  case __NR_open:
    {
      auto path = (const char*)SECCOMP_PARM1(ctx);
      auto flags = (int)SECCOMP_PARM2(ctx);
      auto mode = (mode_t)SECCOMP_PARM3(ctx);
      auto r = bridge.send_open(path, flags, mode);
      SECCOMP_RESULT(ctx) = r;
    }
    break;

  case __NR_openat:
    {
      auto dirfd = (int)SECCOMP_PARM1(ctx);
      auto path = (const char*)SECCOMP_PARM2(ctx);
      auto flags = (int)SECCOMP_PARM3(ctx);
      auto mode = (mode_t)SECCOMP_PARM4(ctx);
      auto r = bridge.send_openat(dirfd, path, flags, mode);
      SECCOMP_RESULT(ctx) = r;
    }
    break;

  case __NR_dup:
    {
      auto fd = (int)SECCOMP_PARM1(ctx);
      auto r = bridge.send_dup(fd);
      SECCOMP_RESULT(ctx) = r;
    }
    break;

  case __NR_dup2:
    {
      auto oldfd = (int)SECCOMP_PARM1(ctx);
      auto newfd = (int)SECCOMP_PARM2(ctx);
      auto r = bridge.send_dup2(oldfd, newfd);
      SECCOMP_RESULT(ctx) = r;
    }
    break;

  case __NR_access:
    {
      auto path = (const char*)SECCOMP_PARM1(ctx);
      auto mode = (int)SECCOMP_PARM2(ctx);
      auto r = bridge.send_access(path, mode);
      SECCOMP_RESULT(ctx) = r;
    }
    break;

  case __NR_fstat:
    {
      auto fd = (int)SECCOMP_PARM1(ctx);
      auto statbuf = (struct stat*)SECCOMP_PARM2(ctx);
      auto r = bridge.send_fstat(fd, statbuf);
      SECCOMP_RESULT(ctx) = r;
    }
    break;

  case __NR_stat:
    {
      auto path = (const char*)SECCOMP_PARM1(ctx);
      auto statbuf = (struct stat*)SECCOMP_PARM2(ctx);
      auto r = bridge.send_stat(path, statbuf);
      SECCOMP_RESULT(ctx) = r;
    }
    break;

  case __NR_lstat:
    {
      auto path = (const char*)SECCOMP_PARM1(ctx);
      auto statbuf = (struct stat*)SECCOMP_PARM2(ctx);
      auto r = bridge.send_lstat(path, statbuf);
      SECCOMP_RESULT(ctx) = r;
    }
    break;

  case __NR_execve:
    {
      LOGU(__NR_execve);
      auto filename = (const char*)SECCOMP_PARM1(ctx);
      auto argv = (char *const*)SECCOMP_PARM2(ctx);
      auto envp = (char *const*)SECCOMP_PARM3(ctx);
      auto r = bridge.send_execve(filename, argv, envp);
      SECCOMP_RESULT(ctx) = r;
      // Call execve() at the handler after leaving the handler and
      // returning to the user space code.
      if (r == 0) {
        static char altstack[1024];
        auto saved_rsp = SECCOMP_REG(ctx, REG_RSP);
        SECCOMP_REG(ctx, REG_RSP) = (long long unsigned int)(altstack + 1024 - sizeof(void*));

        install_fakeframe(ctx, (void*)execve_handler, (void*)saved_rsp);

        // set arguments for the handler
        SECCOMP_REG(ctx, REG_RDI) = (long long unsigned int)filename;
        SECCOMP_REG(ctx, REG_RSI) = (long long unsigned int)argv;
        SECCOMP_REG(ctx, REG_RDX) = (long long unsigned int)envp;
      }
    }
    break;

  case __NR_readlink:
    {
      auto path = (const char*)SECCOMP_PARM1(ctx);
      auto buf = (char*)SECCOMP_PARM2(ctx);
      auto bufsize = (size_t)SECCOMP_PARM3(ctx);
      auto r = bridge.send_readlink(path, buf, bufsize);
      SECCOMP_RESULT(ctx) = r;
    }
    break;

  case __NR_unlink:
    {
      auto path = (const char*)SECCOMP_PARM1(ctx);
      auto r = bridge.send_unlink(path);
      SECCOMP_RESULT(ctx) = r;
    }
    break;

  case __NR_vfork:
    {
      LOGU(__NR_vfork);
#if 0
      auto r = bridge.send_vfork();
#endif
      auto r = 0L;
      SECCOMP_RESULT(ctx) = r;
      // Call vfork() after leaving the handler, and return to the
      // user space code.
      static char altstack[1024];
      auto saved_rsp = SECCOMP_REG(ctx, REG_RSP);
      SECCOMP_REG(ctx, REG_RSP) = (long long unsigned int)(altstack + 1024 - sizeof(void*));

      install_fakeframe(ctx, (void*)vfork_handler, (void*)saved_rsp);
      SECCOMP_PARM1(ctx) = SECCOMP_REG(ctx, REG_RSP);
      SECCOMP_PARM2(ctx) = (long long unsigned int)(altstack + 1024 - sizeof(void*));
    }
    break;

  case __NR_rt_sigaction:
    {
      auto signum = (int)SECCOMP_PARM1(ctx);
      auto act = (const struct sigaction*)SECCOMP_PARM2(ctx);
      auto oldact = (struct sigaction*)SECCOMP_PARM3(ctx);
      auto sigsetsz = (size_t)SECCOMP_PARM4(ctx);
      auto r = bridge.send_rt_sigaction(signum, act, oldact, sigsetsz);
      SECCOMP_RESULT(ctx) = r;
    }
    break;
  }
}

static void
sigsys(int nr, siginfo_t *info, void* void_context) {
  ucontext_t *ctx = (ucontext_t*)void_context;
  handle_syscall(info, ctx);
}

void
install_seccomp_sigsys(int ccsock) {
  bridge.set_sock(ccsock);

  struct sigaction act;
  bzero(&act, sizeof(act));
  act.sa_sigaction = &sigsys;
  act.sa_flags = SA_SIGINFO;
  int r = sigaction(SIGSYS, &act, nullptr);
  if (r < 0) {
    perror("sigaction");
    abort();
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
void
install_seccomp_filter() {
  install_filter();
}
