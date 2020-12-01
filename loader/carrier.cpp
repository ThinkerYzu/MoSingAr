/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "carrier.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

#define _E(x, args...) \
  do { \
    auto r = x(args); \
    if (r < 0) { perror(#x); abort(); } \
  } while (0)


carrier::carrier() {
  int socks[2];
  _E(socketpair, AF_UNIX, SOCK_DGRAM, 0, socks);
  _E(dup2, socks[1], CARRIER_SOCK);
  _E(close, socks[1]);
  _E(fcntl, socks[0], F_SETFD, FD_CLOEXEC);

  cc = new cmdcenter(socks[0]);
  cc->init();
}

carrier::~carrier() {
  if (cc) {
    delete cc;
  }
}

int
carrier::run(int argc, char * const * argv) {
  return cc->start_mission(argc, argv);
}

void
carrier::handle_messages() {
  cc->handle_messages();
}

void
carrier::stop_msg_loop() {
  cc->stop_msg_loop();
}
