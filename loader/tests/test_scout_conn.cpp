/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "scout.h"
#include "cmdcenter.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>


int
main(int argc, char * const * argv) {
  int socks[2];
  auto r = socketpair(AF_UNIX, SOCK_DGRAM, 0, socks);
  if (r < 0) {
    perror("socketpair");
    return 255;
  }

  r = dup2(socks[1], CARRIER_SOCK);
  if (r < 0) {
    perror("dup2");
    return 255;
  }
  close(socks[1]);

  cmdcenter cc(socks[0]);
  if (!cc.init()) {
    printf("ERR\n");
    return 255;
  }

  scout sct1;
  sct1.connect_cmdcenter();
  scout sct2;
  sct2.connect_cmdcenter();
  scout sct3;
  sct3.connect_cmdcenter();

  cc.handle_message();
  cc.handle_message();
  cc.handle_message();

  assert(cc.get_num_scouts() == 3);
  printf("OK\n");
  return 0;
}
