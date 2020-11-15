/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>

int main(int argc, const char* argv[]) {
  int fd = dup(2);
  if (fd < 0) {
    perror("dup");
    return 255;
  }
  auto str = "ok\n";
  write(1, str, strlen(str));
  return 0;
}

