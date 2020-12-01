/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "scout.h"

#include "new"

#include <stdlib.h>
#include <unistd.h>

#define assert(x) do { if (!(x)) { abort(); } } while(0)


extern "C" {
extern void tinymalloc_init();

unsigned long int global_flags __attribute__((visibility("default"))) = (unsigned long int)&global_flags;
}

class bootstrap {
public:
  bootstrap() {
    // Neutralize the effects caused by the relocation.
    // Check the comment in the body of prepare_shellcode().
    global_flags -= (unsigned long int)&global_flags;

    // Use a buf since tinymalloc is not ready yet.
    sct = new((void*)sct_buf) scout();

    auto syscall_r = sct->install_syscall_trampo();
    assert(syscall_r);

    if (!(global_flags & scout::FLAG_CC_COMM_READY)) {
      sct->establish_cc_channel();
    }

    tinymalloc_init();

    auto sigsys_r = sct->install_sigsys();
    assert(sigsys_r);
    if (!(global_flags & scout::FLAG_FILTER_INSTALLED)) {
      auto filter_r = sct->install_seccomp_filter();
      assert(filter_r);
    }
  }

private:
  scout *sct;
  char sct_buf[sizeof(scout)];
};

static bootstrap _bootstrap;
