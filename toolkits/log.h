/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __log_h_
#define __log_h_

#include <unistd.h>

#if DEBUG
#define LOGU(x) do { auto msg = #x "\n"; write(1, msg, strlen(msg)); } while (0)
#else
#define LOGU(x)
#endif

#endif /* __log_h_ */
