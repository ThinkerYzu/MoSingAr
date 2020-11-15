/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __bridge_h_
#define __bridge_h_

#include <sys/types.h>
#include <unistd.h>

class sandbox_bridge {
public:
  int send_open(const char* path, int flags, mode_t mode);
  int send_openat(int dirfd, const char* path, int flags, mode_t mode);
  int send_dup(int oldfd);
  int send_dup2(int oldfd, int newfd);
  int send_access(const char* path, int mode);
  int send_fstat(int fd, struct stat* statbuf);
  int send_stat(const char* path, struct stat* statbuf);
  int send_lstat(const char* path, struct stat* statbuf);
  int send_execve(const char* filename, char*const* argv, char*const* envp);
  size_t send_readlink(const char* path, char* buf, size_t bufsize);
  int send_unlink(const char* path);
  pid_t send_vfork();

private:
  int sock;
};

#endif /* __bridge_h_ */
