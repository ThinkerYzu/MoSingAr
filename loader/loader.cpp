/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "loader.h"

#include <sys/types.h>
#include <unistd.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/fcntl.h>

#define PG_SZ 4096
// The address of the syscall trampoline that the seccomp filter will
// skip it.
#define TRAMPOLINE_ADDR 0x200000000000

void loader_start() {}

#ifdef __x86_64__
static long
__syscall(long nr,
          long arg1 = 0,
          long arg2 = 0,
          long arg3 = 0,
          long arg4 = 0,
          long arg5 = 0,
          long arg6 = 0) {
  long r;
  asm(" \
movq %1, %%rax; \
movq %2, %%rdi; \
movq %3, %%rsi; \
movq %4, %%rdx; \
movq %5, %%r10; \
movq %6, %%r8; \
movq %7, %%r9; \
syscall; \
movq %%rax, %0;"
      :"=r"(r)
      :"m"(nr), "m"(arg1), "m"(arg2), "m"(arg3), "m"(arg4), "m"(arg5), "m"(arg6)
      :"rax", "rdi", "rsi", "rdx", "r10", "r8", "r9"
      );
  return r;
}
#else
#error "Unknown platform"
#endif

static long
_syscall(long nr,
         long arg1 = 0,
         long arg2 = 0,
         long arg3 = 0,
         long arg4 = 0,
         long arg5 = 0,
         long arg6 = 0) {
  auto trampo = (long(*)(long, long, long, long, long, long, long))TRAMPOLINE_ADDR;
  return trampo(nr, arg1, arg2, arg3, arg4, arg5, arg6);
}

long
load_shared_object(const char* path, prog_header* headers, int header_num,
                   void (**init_array)(),
                   void** rela,
                   long flags) {
  // For the seccomp filter may have been installed, setup the syscall
  // trampoline to skip the check.
  auto syscall_tramp = __syscall(__NR_mmap,
                                 TRAMPOLINE_ADDR,
                                 4096,
                                 PROT_READ | PROT_WRITE | PROT_EXEC,
                                 MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                                 -1,
                                 0);
  auto trampo_sz = (char*)_syscall - (char*)__syscall;
  for (int i = 0; i < trampo_sz; i++) {
    ((char*)syscall_tramp)[i] = ((char*)__syscall)[i];
  }

  auto fd = _syscall(__NR_open, (long)path, (long)O_RDONLY);
  if (fd < 0) {
    return fd;
  }
  long r = 0;

  if (headers[0].offset != 0 || headers[0].addr != 0) {
    // Assume the first segment always starts from the begin of
    // the file and positioned at the address of 0.
    _syscall(__NR_exit, 255);
  }

  // Map memory and load content from the SO.
  long msz = 0;
  int i;
  for (i = 0; i < header_num; i++) {
    auto header = headers + i;

    auto map_stop = (header->addr + header->mem_size + PG_SZ - 1) & ~(PG_SZ - 1);
    if (map_stop > msz) {
      msz = map_stop;
    }
  }

  auto map = _syscall(__NR_mmap,
                      0, msz,
                      PROT_EXEC | PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if ((long)map < 0) {
    return (long)map;
  }

  for (i = 0; i < header_num; i++) {
    auto header = headers + i;

    auto seg_start = (unsigned long)map + header->addr;

    r = _syscall(__NR_lseek, fd, header->offset, SEEK_SET);
    if (r < 0) {
      return r;
    }
    r = _syscall(__NR_read, fd, seg_start, header->file_size);
    if (r < 0) {
      return r;
    }
  }

  auto rela_p = rela;
  while (*rela_p) {
    auto offset = (long)rela_p[0];
    auto addend = (long)rela_p[1];
    auto pptr = (void**)((char*)map + offset);
    *pptr = (void*)((char*)map + addend);
    rela_p += 2;
  }

  r = _syscall(__NR_close, fd);
  if (r < 0) {
    return r;
  }

  // Call init functions
  auto init_p = init_array;
  while (*init_p) {
    auto init_func = (void(*)())((char*)map + (long)*init_p);
    init_func();
    init_p++;
  }

  return 0;
}

void loader_end() {}
