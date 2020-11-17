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

/**
 * Trampoline will be placed at this fix address.
 *
 * Trampoline is a copy of |syscall_trampoline()|.  It always stays at
 * the same fixed address.  All syscalls made through the trampoline
 * will always be allowed by the filter.  The filter check if a
 * syscall is from the trampoline by checking it's instruction
 * pointer.  It should be in the range of TRAMPOLINE_ADDR ~ +4096
 * bytes.
 *
 * |syscall_trampoline()| is another implementation of |syscall()|,
 * however it return negative error code instead of -1 and putting the
 * (postive) error code in |errno|, like |syscall()|.  The returned
 * value of |syscall_trampoline()| should be resturned by the signal
 * handler so that it can set a correct value to |errno|.
 */
#define TRAMPOLINE_ADDR 0x200000000000

#endif /* __bridge_h_ */
