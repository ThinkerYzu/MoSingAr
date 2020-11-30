/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "bridge.h"
#include "scout.h"
#include "tinypack.h"

#include <asm/unistd.h>

#include <memory>
#include <assert.h>


extern "C"
const char trampoline_progm[] = "./tdtrampoline";

extern "C" {
extern long (*td__syscall_trampo)(long, ...);
}


int sandbox_bridge::send_open(const char* path, int flags, mode_t mode) {
  auto pack = tinypacker()
    .field(scout::cmd_open)
    .field(path)
    .field(flags)
    .field(mode);
  auto buf = pack.pack_size_prefix();
  write(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

int sandbox_bridge::send_openat(int dirfd, const char* path, int flags, mode_t mode) {
  auto pack = tinypacker()
    .field(scout::cmd_openat)
    .field(dirfd)
    .field(path)
    .field(flags)
    .field(mode);
  auto buf = pack.pack_size_prefix();
  write(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

int sandbox_bridge::send_dup(int oldfd) {
  auto r = td__syscall_trampo(__NR_dup, oldfd);
  return r;
}

int sandbox_bridge::send_dup2(int oldfd, int newfd) {
  auto r = td__syscall_trampo(__NR_dup2, oldfd, newfd);
  return r;
}

int sandbox_bridge::send_access(const char* path, int mode) {
  auto pack = tinypacker()
    .field(scout::cmd_access)
    .field(path)
    .field(mode);
  auto buf = pack.pack_size_prefix();
  write(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

int sandbox_bridge::send_fstat(int fd, struct stat* statbuf) {
  auto pack = tinypacker()
    .field(scout::cmd_fstat)
    .field(fd)
    .field(statbuf);
  auto buf = pack.pack_size_prefix();
  write(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

int sandbox_bridge::send_stat(const char* path, struct stat* statbuf) {
  auto pack = tinypacker()
    .field(scout::cmd_stat)
    .field(path)
    .field(statbuf);
  auto buf = pack.pack_size_prefix();
  write(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

int sandbox_bridge::send_lstat(const char* path, struct stat* statbuf) {
  auto pack = tinypacker()
    .field(scout::cmd_lstat)
    .field(path)
    .field(statbuf);
  auto buf = pack.pack_size_prefix();
  write(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

/**
 * The bridge handles |execve()| by executing a trampoline program,
 * that in turn will execute the program that the caller want to
 * execute after setting |LD_PRELOAD| environment variable properly to
 * make sure libtongdao.so being loaded and setting a sandbox.
 *
 * The filter will trap all |execve()| calls except ones running the
 * trampoline program.
 */
int sandbox_bridge::send_execve(const char* filename, char*const* argv, char*const* envp) {
  auto r = td__syscall_trampo(__NR_execve, trampoline_progm, argv, envp);
  return r;
}

size_t sandbox_bridge::send_readlink(const char* path, char* _buf, size_t _bufsize) {
  auto pack = tinypacker()
    .field(scout::cmd_readlink)
    .field(path);
  auto buf = pack.pack_size_prefix();
  write(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

int sandbox_bridge::send_unlink(const char* path) {
  auto pack = tinypacker()
    .field(scout::cmd_unlink)
    .field(path);
  auto buf = pack.pack_size_prefix();
  write(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}

pid_t sandbox_bridge::send_vfork() {
  auto pack = tinypacker()
    .field(scout::cmd_vfork);
  auto buf = pack.pack_size_prefix();
  write(sock, buf, pack.get_size_prefix());
  free(buf);
  return 0;
}
