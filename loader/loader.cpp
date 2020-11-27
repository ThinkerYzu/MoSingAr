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

void loader_start() {}

#ifdef __x86_64__
static long
_syscall(long nr,
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

long
load_shared_object(const char* path, prog_header* headers, int header_num,
                   void (**init_array)(),
                   void** rela,
                   long flags) {
  auto fd = _syscall(__NR_open, (long)path, (long)O_RDONLY);
  if (fd < 0) {
    return fd;
  }
  long r = 0;

  // Map memory and load content from the SO.
  void *first_map = nullptr;
  int i;
  for (i = 0; i < header_num; i++) {
    auto header = headers + i;

    auto map_start = ((unsigned long)first_map + header->addr) & ~(PG_SZ - 1);
    auto map_stop = ((unsigned long)first_map + header->addr +
                     header->mem_size + PG_SZ - 1) & ~(PG_SZ - 1);
    auto msz = map_stop - map_start;
    auto ptr = _syscall(__NR_mmap,
                        map_start, msz,
                        PROT_EXEC | PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if ((long)ptr < 0) {
      return (long)ptr;
    }
    if (first_map == nullptr) {
      first_map = (void*)ptr;
      if (header->offset != 0 || header->addr != 0) {
        // Assume the first segment always starts from the begin of
        // the file and positioned at the address of 0.
        _syscall(__NR_exit, 255);
      }
    } else {
      ptr = (long)first_map + header->addr;
    }

    r = _syscall(__NR_lseek, fd, header->offset, SEEK_SET);
    if (r < 0) {
      return r;
    }
    r = _syscall(__NR_read, fd, ptr, header->file_size);
    if (r < 0) {
      return r;
    }
  }

  auto rela_p = rela;
  while (*rela_p) {
    auto offset = (long)rela_p[0];
    auto addend = (long)rela_p[1];
    auto pptr = (void**)((char*)first_map + offset);
    *pptr = (void*)((char*)first_map + addend);
    rela_p += 2;
  }

  r = _syscall(__NR_close, fd);
  if (r < 0) {
    return r;
  }

  // Call init functions
  auto init_p = init_array;
  while (*init_p) {
    auto init_func = (void(*)())((char*)first_map + (long)*init_p);
    init_func();
    init_p++;
  }

  return 0;
}

void loader_end() {}
