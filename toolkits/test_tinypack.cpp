/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "tinypack.h"

#include <stdio.h>
#include <assert.h>
#include <string.h>

int
main(int argc, const char * const argv[]) {
  int a = 3;
  double b = 1.1;
  long c = 0x88;
  const char *str = "hello";
  char buf[22];
  for (int i = 0; i < 22; i++) buf[i] = i;
  fixedbuf fbuf(buf, 22);
  auto pack = tinypacker().field(a).field(b).field(c).field(str).field(fbuf);
  printf("pack size %d\n", pack.get_size());
  assert(pack.get_size() == 56);

  auto msg = pack.pack();
  for (int i = 0; i < pack.get_size(); i++) {
    printf("%02x ", 0xff&(int)msg[i]);
  }
  printf("\n");

  int a_;
  double b_;
  long c_;
  const char *str_;
  char _buf[22];
  for (int i = 0; i < 22; i++) _buf[i] = i;
  fixedbuf _fbuf(buf, 22);
  auto unpack = tinyunpacker(msg, pack.get_size()).field(a_).field(b_).field(c_).field(str_).field(_fbuf);
  printf("unpack size %d\n", unpack.get_size());
  assert(pack.get_size() == unpack.get_size());

  assert(unpack.check_completed());

  unpack.unpack();
  printf("a %d, a_ %d\n", a, a_);
  assert(a == a_);
  printf("b %f, b_ %f\n", b, b_);
  assert(b == b_);
  printf("c 0x%lx, c_ 0x%lx\n", c, c_);
  assert(c == c_);
  printf("str %s, str_ %s\n", str, str_);
  assert(strcmp(str, str_) == 0);
  assert(memcmp(buf, _buf, 22) == 0);
}
