/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __carrier_h_
#define __carrier_h_

#include "cmdcenter.h"

/**
 * Carrier creates the first subject, the process of a binary, and
 * initialize a sandbox for it and all subjects forked from it.
 *
 * For the first subject, it should be forked from the Carrier and
 * exec a target binary.  Just before executing the binary, the scout
 * of the first subject will enable sandbox for the process.  The
 * process of enabling the sandbox comprises the isntallation of a
 * seccomp filter and the isntallation of the signal handler for
 * SIGSYS and create a socket pair to establish the communicatoin
 * channel to the Command Center on the Carrier.
 *
 * Every time forking a new subject except the first one, the scout of
 * the subject should create a new socket pair and pass one of two
 * ends of the pair to the Command Center to establish a private
 * communication channel.
 *
 * Every time executing a binary, the scout of the subject will notify
 * the Command Center to monitor/trace the subject.  That makes the
 * Command Center to stop the subject, a process, after exec, and
 * inject a loader to the subject to install a signal handler for
 * SIGSYS, since a process will lost its signal handlers after exec.
 * However, it reuses the socket, that was created before calling
 * exec, to the Command Center.  To reach it, the private socket to
 * the Command Center is always placed at a predefined FD number.
 *
 */
class carrier {
public:
  carrier();
  ~carrier();

  /**
   * Run a missions and start monitoring and deceiving it and all
   * off-spring subjects.
   */
  int run(int argc, char * const * argv);

  void handle_messages();
  void stop_msg_loop();

private:
  cmdcenter* cc;
};

#endif /* __carrier_h_ */
