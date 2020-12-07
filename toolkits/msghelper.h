/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __msghelper_h_
#define __msghelper_h_

extern int send_msg(int sock, void* buf, int bufsz, int sendfd1 = -1, int sendfd2 = -1);

/**
 * Receive one message a time from the sock that may along with FDs.
 */
class msg_receiver {
public:
  constexpr static int data_buf_size = 1024 * 8;
  // It can handle at most 2 FDs in a message.
  constexpr static int fd_rcvd_size = 2;

  msg_receiver(int fd);
  ~msg_receiver();

  bool receive_one();

  int get_data_bytes() { return data_bytes; }
  char* get_data() { return data; }

  int get_fd_rcvd_num() { return fd_rcvd_num; }
  int* get_fd_rcvd() { return fd_rcvd; }

private:
  int fd;
  int fd_rcvd_num;
  int fd_rcvd[fd_rcvd_size];
  int data_bytes;
  char* data;
};

#endif /* __msghelper_h_ */
