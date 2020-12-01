/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __flightdeck_h_
#define __flightdeck_h_

#include <sys/types.h>

namespace flightdeck {
extern long scout_takeoff(pid_t pid, unsigned long global_flags);
}

#endif /* __flightdeck_h_ */
