/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "seccomp.h"
#include "bridge.h"

#include <stdio.h>
#include <stdint.h>

#include <asm/unistd.h>

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/bpf.h>


static struct sock_filter filter[] = {
  // trampoline
  // Always allow requests made by the trampoline.
  BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
           (offsetof(struct seccomp_data, instruction_pointer))),
  BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, 4096, 2, 0),
  BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
           (offsetof(struct seccomp_data, instruction_pointer) + 4)),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (uint32_t)(TRAMPOLINE_ADDR >> 32), 14, 0),

  // Load syscall number
  BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
           (offsetof(struct seccomp_data, nr))),

  // dup
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_rt_sigaction, 13, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_dup, 12, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_dup2, 11, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open, 10, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_openat, 9, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_access, 8, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_fstat, 7, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_lstat, 6, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_execve, 5, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_readlink, 4, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_stat, 3, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_unlink, 2, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_vfork, 1, 0),
  BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
  BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_TRAP),
};

struct sock_fprog sandbox_filter_prog = {
  .len = ARRAY_SIZE(filter),
  .filter = filter,
};
