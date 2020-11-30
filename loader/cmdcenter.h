/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __cmdcenter_h_
#define __cmdcenter_h_

#include <sys/types.h>
#include <list>

// Use this is the unix socket to talk to the command center.
#define CARRIER_SOCK 73

/**
 * cmdcenter stands for Command Center.  It handles request messages
 * from scouts.  A scout watches and deceives a process isolated in
 * the sandbox.  Once a process is created, the scout for it creates a
 * UNIX socket to eastablish a communication channel with the Command
 * Center.
 *
 * A socket created by a scout will be sent to the Command Center
 * along with a message through the UNIX sockets of the Carrier
 * process, that has a fixed predefined FD number, CARRIER_SOCK.  Once
 * the socket of a scout has been received, the Command Center talk
 * with the scout privately.
 */
class cmdcenter {
public:
  constexpr static int max_events = 16;
  constexpr static int max_cmsg_buf = 64;

  constexpr static int SCOUT_CONNECT_CMD = 0x37fa;
  constexpr static int STOP_MSG_LOOP_CMD = 0x37fb;

  /**
   * \param fd is the socket of the Carrier process.
   */
  cmdcenter(int fd)
    : stopping_message(false)
    , efd(-1)
    , carrierfd(fd) {}
  ~cmdcenter();

  bool init();

  bool handle_message();
  void handle_messages();
  void handle_exec(pid_t pid);
  bool add_scout(int subjecfd);
  bool remove_scout(int scoutfd);

  int get_num_scouts() {
    return scoutfds.size();
  }

  void stop_msg_loop();

private:
  bool handle_carrier_msg();
  bool handle_scout_msg(int sock);

  bool stopping_message;
  int efd;
  int carrierfd;
  std::list<int> scoutfds;
};

#endif /* __cmdcenter_h_ */
