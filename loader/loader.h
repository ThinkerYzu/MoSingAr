/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __loader_h_
#define __loader_h_

struct prog_header {
  long offset;
  long addr;
  long file_size;
  long mem_size;
};

extern void loader_start();
extern long load_shared_object(const char* path,
                               prog_header* headers,
                               int header_num,
                               void (**init_array)(),
                               void** rela,
                               long flags);
extern void loader_end();

#endif /* __loader_h_ */
