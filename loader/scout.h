/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __scout_h_
#define __scout_h_

class scout {
public:
  scout() : sock(-1) {}
  ~scout();

  bool connect_cmdcenter();

private:
  int sock;
};

#endif /* __scout_h_ */
