/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define NONEXISTENT "nonexistent"

int
main(int argc, const char** argv) {
  char *const argv_p[] = {
    strdup(NONEXISTENT),
    NULL
  };
  char *const envp[] = {
    NULL
  };
  auto r = execve(NONEXISTENT, argv_p, envp);
  if (r < 0) {
    perror("execve");
  }
  return 0;
}
