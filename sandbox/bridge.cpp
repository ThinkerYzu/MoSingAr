/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "bridge.h"
#include "bridge_common.h"
#include "tinypack.h"

#include <memory>

int sandbox_bridge::send_open(const char* path, int flags, mode_t mode) {
  auto pack = tinypacker()
    .field(bridge::cmd_open)
    .field(path)
    .field(flags)
    .field(mode);
  std::unique_ptr<char> buf(pack.pack_size_prefix());
  write(sock, buf.get(), pack.get_size_prefix());
  return 0;
}

int sandbox_bridge::send_openat(int dirfd, const char* path, int flags, mode_t mode) {
  auto pack = tinypacker()
    .field(bridge::cmd_openat)
    .field(dirfd)
    .field(path)
    .field(flags)
    .field(mode);
  std::unique_ptr<char> buf(pack.pack_size_prefix());
  write(sock, buf.get(), pack.get_size_prefix());
  return 0;
}

int sandbox_bridge::send_access(const char* path, int mode) {
  auto pack = tinypacker()
    .field(bridge::cmd_access)
    .field(path)
    .field(mode);
  std::unique_ptr<char> buf(pack.pack_size_prefix());
  write(sock, buf.get(), pack.get_size_prefix());
  return 0;
}

int sandbox_bridge::send_fstat(int fd, struct stat* statbuf) {
  auto pack = tinypacker()
    .field(bridge::cmd_fstat)
    .field(fd)
    .field(statbuf);
  std::unique_ptr<char> buf(pack.pack_size_prefix());
  write(sock, buf.get(), pack.get_size_prefix());
  return 0;
}

int sandbox_bridge::send_stat(const char* path, struct stat* statbuf) {
  auto pack = tinypacker()
    .field(bridge::cmd_stat)
    .field(path)
    .field(statbuf);
  std::unique_ptr<char> buf(pack.pack_size_prefix());
  write(sock, buf.get(), pack.get_size_prefix());
  return 0;
}

int sandbox_bridge::send_lstat(const char* path, struct stat* statbuf) {
  auto pack = tinypacker()
    .field(bridge::cmd_lstat)
    .field(path)
    .field(statbuf);
  std::unique_ptr<char> buf(pack.pack_size_prefix());
  write(sock, buf.get(), pack.get_size_prefix());
  return 0;
}

int sandbox_bridge::send_execve(const char* filename, char*const* argv, char*const* envp) {
  auto pack = tinypacker()
    .field(bridge::cmd_execve)
    .field(filename)
    .field(argv)
    .field(envp);
  std::unique_ptr<char> buf(pack.pack_size_prefix());
  write(sock, buf.get(), pack.get_size_prefix());
  return 0;
}

size_t sandbox_bridge::send_readlink(const char* path, char* _buf, size_t _bufsize) {
  auto pack = tinypacker()
    .field(bridge::cmd_readlink)
    .field(path);
  std::unique_ptr<char> buf(pack.pack_size_prefix());
  write(sock, buf.get(), pack.get_size_prefix());
  return 0;
}

int sandbox_bridge::send_unlink(const char* path) {
  auto pack = tinypacker()
    .field(bridge::cmd_unlink)
    .field(path);
  std::unique_ptr<char> buf(pack.pack_size_prefix());
  write(sock, buf.get(), pack.get_size_prefix());
  return 0;
}

pid_t sandbox_bridge::send_vfork() {
  auto pack = tinypacker()
    .field(bridge::cmd_vfork);
  std::unique_ptr<char> buf(pack.pack_size_prefix());
  write(sock, buf.get(), pack.get_size_prefix());
  return 0;
}
