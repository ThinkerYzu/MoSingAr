/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "carrier.h"

#include "log.h"

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>

static pid_t childpid = -1;
static carrier *carrier_ptr = nullptr;
static int exitval = 255;
extern bool sigchld_ignore;

void
sigchld_handler(int signum, siginfo_t* info, void* ucontext) {
  assert(signum == SIGCHLD);
  if (info->si_pid != childpid || sigchld_ignore) {
    return;
  }
  auto status = info->si_status;
  if (WIFSTOPPED(status)) {
    printf("The child %d has been stopped by signum %d\n", info->si_pid, WSTOPSIG(status));
  } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
    if (WIFSIGNALED(status)) {
      if (WTERMSIG(status) == SIGSYS || WTERMSIG(status) == SIGSTOP) {
        LOGU(SIGSYS || SIGSTOP);
        return;
      }
      printf("The child %d is terminated for signum %d\n", info->si_pid, WTERMSIG(status));
    } else {
      exitval = WEXITSTATUS(status) & 0xff;
    }
    carrier_ptr->stop_msg_loop();
  }
}

int
main(int argc, char*const* argv) {
  carrier crr;
  carrier_ptr = &crr;

  auto pid = crr.run(argc - 1, argv + 1);

  // Install a SIGCHLD handler to terminate the Carrier when the
  // mission is completed.
  childpid = pid;
  struct sigaction act;
  bzero(&act, sizeof(act));
  act.sa_sigaction = sigchld_handler;
  act.sa_flags = SA_SIGINFO;
  auto r = sigaction(SIGCHLD, &act, nullptr);
  if (r < 0) {
    perror("sigaction");
    return 255;
  }

  // Check if the mission is completed in case a SIGCHLD signal has
  // been emitted before the signal handler being ready.
  int status;
  r = waitpid(childpid, &status, WNOHANG);
  if (r < 0 || WIFEXITED(status)) {
    return 0;
  }

  crr.handle_messages();

  return exitval;
}
