/* -*- Mode: C++; Tab-Width: 8; Indent-Tabs-Mode: Nil; C-Basic-Offset: 2 -*-
 * Vim: Set Ts=8 Sts=2 Et Sw=2 Tw=80:
 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#define CARRIER_SOCK 73

/**
 * This program creates a socket pairt to simulate the Carrier's
 * socket, that is required by scouts to eastablish communication
 * channel with the Command Center.
 */
int
main(int argc, char*const* argv) {
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
  r = execv(argv[1], argv + 1);
  if (r < 0) {
    perror("execv");
    return 255;
  }
  return 0;
}
