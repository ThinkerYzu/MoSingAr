/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __errhandle_h_
#define __errhandle_h_

#include <stdio.h>
#include <stdlib.h>

#define _E(name, args...)                                   \
  do {                                                      \
    auto __errhandle_r = name(args);                        \
    if (__errhandle_r < 0) { perror(#name); return false; } \
  } while( 0)
#define _EI(name, args...)                                              \
  do {                                                                  \
    auto __errhandle_r = name(args);                                    \
    if (__errhandle_r < 0) { perror(#name); return __errhandle_r; }     \
  } while( 0)
#define _EA(name, args...)                                  \
  do {                                                      \
    auto __errhandle_r = name(args);                        \
    if (__errhandle_r < 0) { perror(#name); abort(); }      \
  } while( 0)
#define _ENull(name, args...) do {                              \
    auto __errhandle_r = name(args);                            \
    if (__errhandle_r < 0) { perror(#name); return nullptr; }   \
  } while( 0)

#define ABORT(msg) do {                                             \
    fprintf(stderr, "ABORT: %s:%d %s\n", __FILE__, __LINE__, msg);  \
    abort();                                                        \
  } while(0)


#endif /* __errhandle_h_ */
