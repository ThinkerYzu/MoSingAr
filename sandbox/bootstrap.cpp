/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <string.h>

#include <assert.h>


extern int init_seccomp();
extern "C" {
long (*td__syscall_trampo)(long, ...);
extern void syscall_trampoline();
extern void tinymalloc_init();
}

class bootstrap {
public:
  bootstrap() {
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

    tinymalloc_init();
    init_seccomp();
  }
};

static bootstrap _bootstrap;
