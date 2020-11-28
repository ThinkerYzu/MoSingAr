/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "scout.h"
#include "cmdcenter.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

scout::~scout() {
  if (sock != -1) {
    close(sock);
  }
}

bool
scout::connect_cmdcenter() {
  int socks[2];
  auto r = socketpair(AF_UNIX, SOCK_DGRAM, 0, socks);
  if (r < 0) {
    perror("socketpair");
    return false;
  }

  int cmd = cmdcenter::SCOUT_CONNECT_CMD;
  iovec iov = {
    .iov_base = &cmd,
    .iov_len = sizeof(cmd)
  };
  char cmsg_buf[CMSG_SPACE(sizeof(int))];
  msghdr msg = { 0 };
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);
  auto cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  *(int*)CMSG_DATA(cmsg) = socks[1];
  r = sendmsg(CARRIER_SOCK, &msg, 0);
  if (r < 0) {
    perror("sendmsg");
    close(socks[0]);
    close(socks[1]);
    return false;
  }

  close(socks[1]);
  sock = socks[0];

  return true;
}
