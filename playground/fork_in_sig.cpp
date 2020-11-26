/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

void
sighandler(int signum) {
  fork();
}

/**
 * This program is an experiment to prove that forking at a signal
 * handler can still return to the control flow that was interrupted.
 */
int
main(int argc, char*const* argv) {
  signal(SIGUSR1, sighandler);
  printf("try \"kill -USR1 %d\" to create a child.\n", getpid());

  for (int i = 0; i < 30; i++) {
    printf("Hi %d\n", getpid());
    sleep(1);
  }
}
