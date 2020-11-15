/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "bridge.h"

int sandbox_bridge::send_open(const char* path, int flags, mode_t mode) {
    return 0;
}

int sandbox_bridge::send_openat(int dirfd, const char* path, int flags, mode_t mode) {
    return 0;
}

int sandbox_bridge::send_access(const char* path, int mode) {
  return 0;
}

int sandbox_bridge::send_fstat(int fd, struct stat* statbuf) {
  return 0;
}

int sandbox_bridge::send_stat(const char* path, struct stat* statbuf) {
  return 0;
}

int sandbox_bridge::send_lstat(const char* path, struct stat* statbuf) {
  return 0;
}

int sandbox_bridge::send_execve(const char* filename, char*const* argv, char*const* envp) {
  return 0;
}

size_t sandbox_bridge::send_readlink(const char* path, char* buf, size_t bufsize) {
  return 0;
}

int sandbox_bridge::send_unlink(const char* path) {
  return 0;
}

pid_t sandbox_bridge::send_vfork() {
  return 0;
}
