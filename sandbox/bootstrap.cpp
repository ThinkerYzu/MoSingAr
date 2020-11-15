/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAGIC_DUP_FD 254
#define DUMMY_DUP_FD 253


extern int init_seccomp();

class bootstrap {
public:
  bootstrap() {
    auto fd = open("/dev/null", O_RDONLY);
    if (fd < 0) {
      perror("open");
      abort();
    }
    bool noclose = false;
    if (fd != DUMMY_DUP_FD) {
      auto r = dup2(fd, DUMMY_DUP_FD);
      if (r < 0) {
        perror("dup2");
        abort();
      }
    } else {
      noclose = true;
    }
    if (fd != MAGIC_DUP_FD) {
      auto r = dup2(fd, MAGIC_DUP_FD);
      if (r < 0) {
        perror("dup2");
        abort();
      }
    } else {
      noclose = true;
    }
    if (!noclose) {
      close(fd);
    }

    init_seccomp();
  }
};

static bootstrap _bootstrap;
