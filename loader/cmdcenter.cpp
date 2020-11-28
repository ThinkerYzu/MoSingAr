/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "cmdcenter.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <assert.h>

#include <algorithm>


cmdcenter::~cmdcenter() {
  for (auto itr = scoutfds.begin();
       itr != scoutfds.end();
       ++itr) {
    close(*itr);
  }
  close(efd);
}

bool
cmdcenter::init() {
  efd = epoll_create1(EPOLL_CLOEXEC);
  if (efd < 0) {
    perror("epoll_create1");
    return false;
  }

  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = carrierfd;
  auto r = epoll_ctl(efd, EPOLL_CTL_ADD, carrierfd, &ev);
  if (r < 0) {
    perror("epoll_ctl");
    return false;
  }

  return true;
}

bool
cmdcenter::handle_carrier_msg() {
  char data_buf[16];
  char cmsg_buf[max_cmsg_buf];
  iovec vec = { data_buf, 16 };
  msghdr msg = { nullptr, 0, &vec, 1, cmsg_buf, max_cmsg_buf, 0 };
  auto bytes = recvmsg(carrierfd, &msg, MSG_DONTWAIT);
  assert(bytes == sizeof(int));
  int cmd = *(int*)data_buf;
  if (cmd == SCOUT_CONNECT_CMD) {
    auto cmsg = CMSG_FIRSTHDR(&msg);
    assert(cmsg != nullptr);
    assert(cmsg->cmsg_level == SOL_SOCKET);
    assert(cmsg->cmsg_type == SCM_RIGHTS);
    assert(cmsg->cmsg_len == CMSG_LEN(sizeof(int)));
    int sock = *(int*)CMSG_DATA(cmsg);
    add_scout(sock);
  } else if (cmd == STOP_MSG_LOOP_CMD) {
    stopping_message = true;
  }

  return true;
}

bool
cmdcenter::handle_message() {
  epoll_event events[max_events];

  auto num = epoll_wait(efd, events, max_events, -1);
  if (num < 0) {
    perror("epoll_wait");
    return false;
  }

  for (auto i = 0; i < num; i++) {
    auto ev = events + i;
    if (ev->events & EPOLLRDHUP) {
      remove_scout(ev->data.fd);
    } else if (ev->events & EPOLLIN) {
      auto sock = ev->data.fd;
      if (sock == carrierfd) {
        auto r = handle_carrier_msg();
        if (!r) {
          return r;
        }
      }
    }
  }

  return true;
}

void
cmdcenter::handle_messages() {
  while (!stopping_message && handle_message()) {
  }
  stopping_message = false;
}

void
cmdcenter::handle_exec(pid_t pid) {
}

bool
cmdcenter::add_scout(int scoutfd) {
  assert(find(scoutfds.begin(), scoutfds.end(), scoutfd) == scoutfds.end());
  scoutfds.push_back(scoutfd);
  epoll_event ev;
  ev.events = EPOLLIN | EPOLLRDHUP;
  ev.data.fd = scoutfd;
  auto r = epoll_ctl(efd, EPOLL_CTL_ADD, scoutfd, &ev);
  if (r < 0) {
    perror("epoll_ctl");
    return false;
  }
  return true;
}

bool
cmdcenter::remove_scout(int scoutfd) {
  bool success = false;
  scoutfds.remove_if([&](const int& v) {
      if (v == scoutfd) {
        auto r = epoll_ctl(efd, EPOLL_CTL_DEL, scoutfd, nullptr);
        if (r < 0) {
          perror("epoll_ctl");
          return false;
        }
        success = true;
      }
      return success;
    });
  return success;
}

void
cmdcenter::stop_msg_loop() {
  int cmd = STOP_MSG_LOOP_CMD;
  send(CARRIER_SOCK, &cmd, sizeof(cmd), 0);
}
