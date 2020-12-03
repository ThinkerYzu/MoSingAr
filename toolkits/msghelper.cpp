/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "msghelper.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#define assert(x)                  \
  do {                             \
    if (!(x)) {                    \
      write(2, "assertion: ", 11); \
      write(2, #x, strlen(#x));    \
      char lf = '\n';              \
      write(2, &lf, 1);            \
      abort();                     \
    }                              \
  } while(0)


int
send_msg(int sock, void* buf, int bufsz, int sendfd1, int sendfd2) {
  auto cmsgcnt = 0;
  if (sendfd1 >= 0) cmsgcnt++;
  if (sendfd2 >= 0) cmsgcnt++;

  auto cmsgsz = CMSG_SPACE(sizeof(int) * cmsgcnt);
  char _cmsg_buf[cmsgsz];
  char* cmsg_buf = _cmsg_buf;
  if (cmsgcnt == 0) {
    cmsg_buf = nullptr;
    cmsgsz = 0;
  }
  iovec vec = { buf, (size_t)bufsz };
  msghdr msg = { nullptr, 0, &vec, 1, cmsg_buf, (size_t)cmsgsz, 0 };

  if (cmsgcnt > 0) {
    auto cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * cmsgcnt);
    auto data = (int*)CMSG_DATA(cmsg);
    if (sendfd1) {
      *data++ = sendfd1;
    }
    if (sendfd2) {
      *data++ = sendfd2;
    }
 }

  auto r = sendmsg(sock, &msg, 0);
  if (r < 0) {
    perror("sendmsg");
  }
  return r;
}

msg_receiver::msg_receiver(int fd)
  : fd(fd)
  , fd_rcvd_num(0)
  , data_bytes(0) {
  data = (char*)malloc(data_buf_size);
}

msg_receiver::~msg_receiver() {
  free(data);
}

bool
msg_receiver::receive_one() {
  auto cmsg_buf_sz = CMSG_SPACE(sizeof(int) * fd_rcvd_size);
  char cmsg_buf[cmsg_buf_sz];
  iovec vec = { data, data_buf_size };
  msghdr msg = { nullptr, 0, &vec, 1, cmsg_buf, cmsg_buf_sz, 0 };

  data_bytes = recvmsg(fd, &msg, 0);
  if (data_bytes < 0) {
    perror("msg_receiver: recvmsg");
    return false;
  }
  assert((unsigned)data_bytes >= sizeof(int));
  assert(!(msg.msg_flags & MSG_TRUNC));

  if (msg.msg_controllen >= CMSG_SPACE(sizeof(int))) {
    auto cmsg = CMSG_FIRSTHDR(&msg);
    fd_rcvd_num = cmsg->cmsg_len / CMSG_LEN(sizeof(int));
    assert(fd_rcvd_num <= fd_rcvd_size);
    for (int i = 0; i < fd_rcvd_num; i++) {
      fd_rcvd[i] = ((int*)CMSG_DATA(cmsg))[i];
    }
  }
  return true;
}
