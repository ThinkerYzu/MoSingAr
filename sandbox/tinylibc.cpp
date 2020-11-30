#include <sys/syscall.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <asm/signal.h>

#define NO_ERRNO

extern "C" {

extern long syscall_trampoline(long, ...);
extern void sig_trampoline();

void printptr(void* p) {
  auto addr = (long)p;
  char buf[19];
  buf[0] = '0';
  buf[1] = 'x';
  buf[18] = '\n';
  auto bidx = 2;
  for (auto i = 60; i >= 0; i -= 4) {
    auto v = 0xf & (addr >> i);
    buf[bidx++] = v < 10 ? '0' + v : 'a' + (v - 10);
  }
  syscall_trampoline(__NR_write, 1, (long)buf, 19);
}

void*
memcpy(void* dest, const void* src, size_t n) {
  auto d = (char*)dest;
  auto s = (const char*)src;
  for (size_t i = 0; i < n; i++) {
    *d++ = *s++;
  }
  return d;
}

void *
mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
  auto r = syscall_trampoline(__NR_mmap, addr, length, prot, flags, fd, offset);
#ifndef NO_ERRNO
  if (r < 0) {
    errno = -r;
    r = -1;
  }
#endif
  return (void*)r;
}

ssize_t
write(int fd, const void* buf, size_t count) {
  auto r = syscall_trampoline(__NR_write, fd, buf, count);
#ifndef NO_ERRNO
  if (r < 0) {
    errno = -r;
    r = -1;
  }
#endif
  return r;
}

size_t
strlen(const char* s) {
  if (s == nullptr) {
    return 0;
  }
  int count = 0;
  while (*s++) count++;
  return count;
}

struct kernel_sigaction
{
  void (*k_sa_handler)(int);
  unsigned long sa_flags;
  void (*sa_restorer) (void);
  sigset_t sa_mask;
};


int
sigaction(int signum, const struct sigaction* act, struct sigaction* oldact) {
  struct kernel_sigaction kact;
  struct kernel_sigaction koldact;

  if (act != nullptr) {
    kact.k_sa_handler = act->sa_handler;
    kact.sa_flags = act->sa_flags | SA_RESTORER;
    kact.sa_restorer = &sig_trampoline;
    kact.sa_mask = act->sa_mask;
  }

  auto r = syscall_trampoline(__NR_rt_sigaction, signum, &kact, &koldact, 8);
#ifndef NO_ERRNO
  if (r < 0) {
    errno = -r;
    r = -1;
  }
#endif

  if (oldact != nullptr) {
    oldact->sa_handler = koldact.k_sa_handler;
    oldact->sa_flags = koldact.sa_flags;
    oldact->sa_restorer = koldact.sa_restorer;
  }

  return r;
}

int
prctl(int option, unsigned long arg2, unsigned long arg3,
      unsigned long arg4, unsigned long arg5) {
  arg3 = arg4 = arg5 = 0;
  auto r = syscall_trampoline(__NR_prctl, option, arg2, arg3, arg4, arg5);
#ifndef NO_ERRNO
  if (r < 0) {
    errno = -r;
    r = -1;
  }
#endif
  return r;
}

void
abort() {
#ifdef DEBUG_TRAP
  asm("int3;");
#endif
  syscall_trampoline(__NR_exit, 255);
}

void
perror(const char* s) {
  write(1, s, strlen(s));
  write(1, " error\n", 7);
}

int seccomp(unsigned int operation, unsigned int flags, void *args) {
  auto r = syscall_trampoline(__NR_seccomp, operation, flags, args);
#ifndef NO_ERRNO
  if (r < 0) {
    errno = -r;
    r = -1;
  }
#endif
  return r;
}

int close(int fd) {
  auto r = syscall_trampoline(__NR_close, fd);
#ifndef NO_ERRNO
  if (r < 0) {
    errno = -r;
    r = -1;
  }
#endif
  return r;
}

int socketpair(int domain, int type, int protocol, int sv[2]) {
  auto r = syscall_trampoline(__NR_socketpair, (long)domain, (long)type, (long)protocol, (long)sv);
#ifndef NO_ERRNO
  if (r < 0) {
    errno = -r;
    r = -1;
  }
#endif
  return r;
}

struct msghdr;

int sendmsg(int sockfd, const struct msghdr *msg, int flags) {
  auto r = syscall_trampoline(__NR_sendmsg, (long)sockfd, (long)msg, (long)flags);
#ifndef NO_ERRNO
  if (r < 0) {
    errno = -r;
    r = -1;
  }
#endif
  return r;
}

}
