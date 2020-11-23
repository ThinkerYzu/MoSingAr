/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "ptracetools.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asm/unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

#include <assert.h>
#include <memory>

extern "C" {
extern void shellcode_syscall();
extern void shellcode_syscall_end();
}

static void
mk_syscall_args(user_regs_struct& regs,
                int nr,
                unsigned long long arg1=0,
                unsigned long long arg2=0,
                unsigned long long arg3=0,
                unsigned long long arg4=0,
                unsigned long long arg5=0,
                unsigned long long arg6=0) {
  regs.rax = nr;
  regs.rdi = arg1;
  regs.rsi = arg2;
  regs.rdx = arg3;
  regs.r10 = arg4;
  regs.r8 = arg5;
  regs.r9 = arg6;
}

long
inject_text(pid_t pid, void* addr, void* ptr, unsigned int length) {
  assert((length & 0x7) == 0);

  auto v = (const uint64_t*)ptr;
  auto cnt = length / 8;
  auto vaddr = (uint64_t*)addr;
  for (unsigned int i = 0; i < cnt; i++) {
    auto r = ptrace(PTRACE_POKETEXT, pid, vaddr, *v);
    if (r < 0) {
      perror("ptrace");
      return r;
    }
    v++;
    vaddr++;
  }
  return 0;
}

long
read_text(pid_t pid, void* addr, void* ptr, unsigned int length) {
  assert((length & 0x7) == 0);

  auto v = (uint64_t*)ptr;
  auto cnt = length / 8;
  auto vaddr = (uint64_t*)addr;
  for (unsigned int i = 0; i < cnt; i++) {
    auto r = ptrace(PTRACE_PEEKTEXT, pid, vaddr, nullptr);
    if (errno != 0) {
      return -1;
    }
    *v++ = r;
    vaddr++;
  }
  return 0;
}

long
ptrace_stop(pid_t pid) {
  auto r = ptrace(PTRACE_INTERRUPT, pid, nullptr, nullptr);
  if (r < 0) {
    perror("ptrace");
    return r;
  }

  int status;
  do {
    r = waitpid(pid, &status, 0);
    if (r < 0) {
      perror("waitpid");
      return r;
    }
  } while(!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP);
  return 0;
}

long
ptrace_cont(pid_t pid) {
  auto r = ptrace(PTRACE_CONT, pid, nullptr, nullptr);
  if (r < 0) {
    perror("ptrace");
    return r;
  }
  return 0;
}

long
ptrace_getregs(pid_t pid, user_regs_struct& regs) {
  auto r = ptrace(PTRACE_GETREGS, pid, nullptr, &regs);
  if (r < 0) {
    perror("ptrace");
  }
  return r;
}

long
ptrace_setregs(pid_t pid, const user_regs_struct& regs) {
  auto r = ptrace(PTRACE_SETREGS, pid, nullptr, &regs);
  if (r < 0) {
    perror("ptrace");
  }
  return r;
}

/**
 * Run a syscall at a tracee.
 *
 * The tracee must be stopped/group-stopped at calling,
 * for ex. by SIGSTOP.
 *
 * This function will 
 *  - save the states of the tracee, including registers and code.  Then,
 *  - it injects a tiny block of shell code to call syscall in the
 *    tracee's space, and run it.  At the last,
 *  - it retreives the return value of the syscall, and
 *    restore the state of the tracee.
 */
long
inject_run_syscall(pid_t pid,
                   int nr,
                   unsigned long long arg1,
                   unsigned long long arg2,
                   unsigned long long arg3,
                   unsigned long long arg4,
                   unsigned long long arg5,
                   unsigned long long arg6) {
  // Save registers
  user_regs_struct regs;
  auto r = ptrace_getregs(pid, regs);
  if (r < 0) {
    return r;
  }

  // Save the existing code
  auto codelen = (char*)shellcode_syscall_end - (char*)shellcode_syscall;
  codelen = (codelen + 7) & ~0x7;
  std::unique_ptr<char> saved_code(new char[codelen]);
  r = read_text(pid, (void*)regs.rip, saved_code.get(), codelen);

  // Set registers for the syscall.
  user_regs_struct call_regs;
  call_regs = regs;
  mk_syscall_args(call_regs, nr, arg1, arg2, arg3, arg4, arg5, arg6);
  r = ptrace_setregs(pid, call_regs);
  if (r < 0) {
    return r;
  }

  // Inject the shell code
  std::unique_ptr<char> code(new char[codelen]);
  memcpy(code.get(), (void*)shellcode_syscall, codelen);
  r = inject_text(pid, (void*)regs.rip, code.get(), codelen);
  if (r < 0) {
    return r;
  }

  // Run the shell code
  r = ptrace_cont(pid);
  if (r < 0) {
    return r;
  }
  int status = 0;
  do {
    r = waitpid(pid, &status, 0);
    if (r < 0) {
      perror("waitpid");
      return r;
    }
  } while(!WIFSTOPPED(status) || WSTOPSIG(status) != SIGTRAP);

  // Get the return value
  r = ptrace_getregs(pid, call_regs);
  if (r < 0) {
    return r;
  }

  // Restore code
  r = inject_text(pid, (void*)regs.rip, saved_code.get(), codelen);
  if (r < 0) {
    return r;
  }

  // Restore registers
  r = ptrace_setregs(pid, regs);
  if (r < 0) {
    return r;
  }

  return call_regs.rax;
}

void*
inject_mmap(pid_t pid, void* addr, size_t length,
            int prot, int flags, int fd, off_t offset) {
  return (void*)inject_run_syscall(pid, __NR_mmap,
                                   (unsigned long long)addr, length,
                                   prot, flags, fd, offset);
}

#ifdef TEST

void
child() {
  for (int i = 0; i < 5; i++) {
    printf("Hello from the child %d\n", getpid());
    sleep(1);
  }
  printf("The child exit\n");
}

void
parent(int pid) {
  printf("attach\n");
  auto r = ptrace(PTRACE_ATTACH, pid, nullptr, 0);
  if (r < 0) {
    perror("ptrace");
    exit(255);
  }

  printf("waitpid\n");
  int status;
  do {
    r = waitpid(pid, &status, 0);
    if (r < 0) {
      perror("waitpid");
      exit(255);
    }
  } while(!WIFSTOPPED(status) && WSTOPSIG(status) != SIGTRAP);

  printf("inject_mmap\n");
  auto addr = inject_mmap(pid, nullptr, 8192, PROT_EXEC | PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1,  0);
  if (r < 0) {
    printf("errno %ld\n", -r);
  }
  printf("pid %d\n", pid);
  ptrace_cont(pid);
  if (r == -1) {
    perror("ptrace");
  }
  printf("%p sleep 8s.\n", addr);
  sleep(8);
}

int
main(int argc, char * const argv[]) {
  auto pid = fork();
  if (pid < 0) {
    perror("fork");
    return 255;
  }
  if (pid == 0) {
    child();
  } else {
    parent(pid);
  }
  return 0;
}

#endif /* TEST */
