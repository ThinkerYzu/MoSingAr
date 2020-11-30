/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __scout_h_
#define __scout_h_

/**
 * A scout is responsible for monitoring and deceiving a subject, a
 * process of an application.  It keep passing information of the
 * subject that it is looking at to the Command Center on the Carrier.
 */
class scout {
public:
  scout() : sock(-1) {}
  ~scout();

  bool init_sandbox();

  // Install a trampoline of syscall that the seccomp filter always
  // let it pass.
  bool install_syscall_trampo();
  // Create a socket pair as the channel to the Command Center.
  bool establish_cc_channel();
  // Instll a signal handler for SIGSYS.
  bool install_sigsys();
  // Install a seccomp filter to monitor this subject.
  bool install_seccomp_filter();

  bool prepare_exec();

private:
  int sock;
};

#endif /* __scout_h_ */
