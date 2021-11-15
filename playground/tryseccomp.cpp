/**
 * This is an experiments to make sure the concept and seccomp
 * workable.
 */

#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <ucontext.h>
#include <asm/unistd.h>

#include <sys/prctl.h>

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <linux/audit.h>

#define ARRAY_SIZE(x) ( sizeof(x) / sizeof((x)[0]) )
#define offsetof(t, f) ((long)&((t *)NULL)->f)

#if defined(__x86_64__)

#define SECCOMP_ARCH        AUDIT_ARCH_X86_64

#define SECCOMP_REG(_ctx, _reg) ((_ctx)->uc_mcontext.gregs[(_reg)])
#define SECCOMP_RESULT(_ctx)    SECCOMP_REG(_ctx, REG_RAX)
#define SECCOMP_SYSCALL(_ctx)   SECCOMP_REG(_ctx, REG_RAX)
#define SECCOMP_IP(_ctx)        SECCOMP_REG(_ctx, REG_RIP)
#define SECCOMP_PARM1(_ctx)     SECCOMP_REG(_ctx, REG_RDI)
#define SECCOMP_PARM2(_ctx)     SECCOMP_REG(_ctx, REG_RSI)
#define SECCOMP_PARM3(_ctx)     SECCOMP_REG(_ctx, REG_RDX)
#define SECCOMP_PARM4(_ctx)     SECCOMP_REG(_ctx, REG_R10)
#define SECCOMP_PARM5(_ctx)     SECCOMP_REG(_ctx, REG_R8)
#define SECCOMP_PARM6(_ctx)     SECCOMP_REG(_ctx, REG_R9)
#define SECCOMP_NR_IDX          (offsetof(struct arch_seccomp_data, nr))
#define SECCOMP_ARCH_IDX        (offsetof(struct arch_seccomp_data, arch))
#define SECCOMP_IP_MSB_IDX      (offsetof(struct arch_seccomp_data,           \
                                          instruction_pointer) + 4)
#define SECCOMP_IP_LSB_IDX      (offsetof(struct arch_seccomp_data,           \
                                          instruction_pointer) + 0)
#define SECCOMP_ARG_MSB_IDX(nr) (offsetof(struct arch_seccomp_data, args) +   \
                                 8*(nr) + 4)
#define SECCOMP_ARG_LSB_IDX(nr) (offsetof(struct arch_seccomp_data, args) +   \
                                 8*(nr) + 0)
#endif

int seccomp(unsigned int operation, unsigned int flags, void *args) {
  return syscall(__NR_seccomp, operation, flags, args);
}

static int
install_filter() {
  struct sock_filter filter[] = {
    BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
             (offsetof(struct seccomp_data, nr))),
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open, 1, 0),
    BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_openat, 0, 1),
    BPF_STMT(BPF_RET | BPF_K,
             SECCOMP_RET_TRAP),
    BPF_STMT(BPF_RET | BPF_K,
             SECCOMP_RET_ALLOW),
  };
  struct sock_fprog prog = {
    .len = ARRAY_SIZE(filter),
    .filter = filter,
  };

  prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
  if (seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog)) {
    perror("seccomp");
    return 1;
  }
  return 0;
}

void sigsys(int nr, siginfo_t *info, void* void_context) {
  printf("sigsys %d\n", nr);
  ucontext_t *ctx = (ucontext_t*)void_context;
  SECCOMP_RESULT(ctx) = 111;
}

static void
install_sigsys() {
  struct sigaction act;
  act.sa_sigaction = &sigsys;
  act.sa_flags = SA_SIGINFO | SA_NODEFER;
  int r = sigaction(SIGSYS, &act, NULL);
  if (r < 0) {
    perror("sigaction");
  }
}

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

void* do_test(void *dummy) {
  printf("__NR_open %d\n", __NR_open);
  int fd = open("/dev/null", O_RDONLY);
  printf("FD %d\n", fd);
  return nullptr;
}

int
main(int argc, const char* argv[]) {
  install_sigsys();
  install_filter();

  pthread_t thr;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_create(&thr, &attr, &do_test, nullptr);
  int* r;
  pthread_join(thr, reinterpret_cast<void**>(&r));
  return 0;
}
