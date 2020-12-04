/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include <stdio.h>
#include <unistd.h>

int
main(int argc, char*const* argv) {
  printf("hello\n");
  printf("sleep 30s\n");
  fflush(stdout);
  sleep(30);
  printf("bye\n");
}
