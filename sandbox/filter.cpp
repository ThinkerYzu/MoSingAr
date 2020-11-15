/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "seccomp.h"

#include <stdio.h>

#include <asm/unistd.h>

#include <linux/seccomp.h>
#include <linux/filter.h>

#define MAGIC_DUP_FD 254


static struct sock_filter filter[] = {
  BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
           (offsetof(struct seccomp_data, nr))),

  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_dup, 0, 2),
  BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
           (offsetof(struct seccomp_data, args[1]))),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, MAGIC_DUP_FD, 15, 16),

  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_dup2, 0, 4),
  BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
           (offsetof(struct seccomp_data, args[1]))),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, MAGIC_DUP_FD, 12, 13),
  BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
           (offsetof(struct seccomp_data, args[2]))),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, MAGIC_DUP_FD, 10, 11),

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
