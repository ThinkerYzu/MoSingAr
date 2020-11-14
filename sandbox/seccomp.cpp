/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "seccomp.h"

#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <asm/unistd.h>

#include <sys/prctl.h>

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>

#include <fcntl.h>
#include <sys/stat.h>

static int seccomp(unsigned int operation, unsigned int flags, void *args) {
  return syscall(__NR_seccomp, operation, flags, args);
}

static int
install_filter() {
  extern struct sock_fprog sandbox_filter_prog;

  prctl(PR_SET_NO_NEW_PRIVS, 1);
  if (seccomp(SECCOMP_SET_MODE_FILTER, 0, &sandbox_filter_prog)) {
    perror("seccomp");
    return 1;
  }
  return 0;
}

class sandbox_bridge {
public:
  int send_open(const char* path, int flags, mode_t mode) {
    return 0;
  }
  int send_openat(int dirfd, const char* path, int flags, mode_t mode) {
    return 0;
  }
  int send_access(const char* path, int mode) {
    return 0;
  }
  int send_fstat(int fd, struct stat* statbuf) {
    return 0;
  }
  int send_stat(const char* path, struct stat* statbuf) {
    return 0;
  }
  int send_lstat(const char* path, struct stat* statbuf) {
    return 0;
  }
  int send_execve(const char* filename, char*const* argv, char*const* envp) {
    return 0;
  }
  size_t send_readlink(const char* path, char* buf, size_t bufsize) {
    return 0;
  }
  int send_unlink(const char* path) {
    return 0;
  }
  pid_t send_vfork() {
    return 0;
  }

private:
  int sock;
};

static sandbox_bridge bridge;

static void
handle_syscall(siginfo_t* info, ucontext_t* context) {
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
      auto filename = (const char*)SECCOMP_PARM1(ctx);
      auto argv = (char *const*)SECCOMP_PARM2(ctx);
      auto envp = (char *const*)SECCOMP_PARM3(ctx);
      auto r = bridge.send_execve(filename, argv, envp);
      SECCOMP_RESULT(ctx) = r;
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
      auto r = bridge.send_vfork();
      SECCOMP_RESULT(ctx) = r;
    }
    break;
  }
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
