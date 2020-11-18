#include <sys/syscall.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <asm/signal.h>

extern "C" {

extern long syscall_trampoline(long, ...);
extern void sig_trampoline();

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
  if (r < 0) {
    errno = -r;
    r = -1;
  }
  return (void*)r;
}

ssize_t
write(int fd, const void* buf, size_t count) {
  auto r = syscall_trampoline(__NR_write, fd, buf, count);
  if (r < 0) {
    errno = -r;
    r = -1;
  }
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
  if (r < 0) {
    errno = -r;
    r = -1;
  }

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
  if (r < 0) {
    errno = -r;
    r = -1;
  }
  return r;
}

}
