/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __bridge_common_h_
#define __bridge_common_h_

namespace bridge {

enum bridge_cmd {
  cmd_open,
  cmd_openat,
  cmd_access,
  cmd_fstat,
  cmd_stat,
  cmd_lstat,
  cmd_execve,
  cmd_readlink,
  cmd_unlink,
  cmd_vfork
};

} // bridge

#endif /* __bridge_common_h_ */
