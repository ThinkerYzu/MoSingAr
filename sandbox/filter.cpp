/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "seccomp.h"

#include <stdio.h>

#include <asm/unistd.h>

#include <linux/seccomp.h>
#include <linux/filter.h>


static struct sock_filter filter[] = {
  BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
           (offsetof(struct seccomp_data, nr))),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open, 1, 0),
  BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_openat, 0, 1),
  BPF_STMT(BPF_RET | BPF_K,
           SECCOMP_RET_TRAP),
  BPF_STMT(BPF_RET | BPF_K,
           SECCOMP_RET_ALLOW),
};

struct sock_fprog sandbox_filter_prog = {
  .len = ARRAY_SIZE(filter),
  .filter = filter,
};
