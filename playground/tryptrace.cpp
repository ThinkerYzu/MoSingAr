/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/unistd.h>
#include <asm/unistd.h>
#include <sys/socket.h>


int gettid() {
  return syscall(__NR_gettid);
}

void
fork_at_child() {
  auto pid = fork();
  if (pid > 0) {
    // parent
    printf("process %d go to sleep\n", getpid());
    sleep(3);
    printf("process %d has woke up\n", getpid());
    exit(0);
  }
  if (pid < 0) {
    perror("fork");
    abort();
  }
  printf("process %d\n", getpid());

  char *const argv[] = {
    strdup("/bin/ls"),
    strdup("-l"),
    nullptr
  };
  char *const envp[] = {
    nullptr
  };
  auto r = execve("/bin/ls", argv, envp);
  if (r < 0) {
    perror("execve");
    abort();
  }
}

int
start_fork(int socks[2]) {
  auto pid = fork();
  if (pid != 0) {
    return pid;
  }

  close(socks[0]);
  char buf[1];
  printf("before read\n");
  read(socks[1], buf, 1);
  printf("after read\n");

  fork_at_child();
}


int
main(int argc, char* const argv[]) {
  int socks[2];
  auto err = socketpair(AF_UNIX, SOCK_STREAM, 0, socks);
  if (err < 0) {
    perror("socketpair");
    abort();
  }
  auto pid = start_fork(socks);
  if (pid < 0) {
    return 255;
  }
  printf("The main go to sleep for 3s.\n");
  sleep(3);
  printf("The main has woke up.\n");
  err = ptrace(PTRACE_SEIZE, pid, nullptr,
               PTRACE_O_TRACECLONE | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK);
  if (err < 0) {
    perror("ptrace");
  }

  close(socks[0]);
  close(socks[1]);
  sleep(5);

  printf("waitpid\n");
  int wstatus;
  waitpid(pid, &wstatus, 0);
  printf("wstatus %d %d %d\n", 0xff & (wstatus>> 8), wstatus>>16, SIGTRAP);
  unsigned long newpid;
  err = ptrace(PTRACE_GETEVENTMSG, pid, nullptr, &newpid);
  if (err < 0) {
    perror("ptrace");
    abort();
  }
  printf("new process %ld\n", newpid);
  sleep(1);
  ptrace(PTRACE_CONT, pid, nullptr, nullptr);
  ptrace(PTRACE_CONT, newpid, nullptr, nullptr);
  printf("The main go to sleep 10s.\n");
  sleep(5);
  printf("The main has woke up.\n");
}
