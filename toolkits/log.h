/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __log_h_
#define __log_h_

#include <unistd.h>
#include <string.h>

#if DEBUG
#define LOGU(x)                                 \
  do {                                          \
    char __LOGU_buf[16];                        \
    _I2STR(getpid(), __LOGU_buf);               \
    write(1, __LOGU_buf, strlen(__LOGU_buf));   \
    auto msg = " " #x " (";                     \
    write(1, msg, strlen(msg));                 \
    V_SN();                                     \
    msg = ")\n";                                \
    write(1, msg, strlen(msg));                 \
  } while (0)
#else
#define LOGU(x)
#endif

#define _I2STR(x, b)                                    \
  do {                                                  \
    auto _i2str_x = (int)(x);                           \
    if (_i2str_x == 0) {                                \
      b[0] = '0'; b[1] = 0;                             \
      break;                                            \
    }                                                   \
    char* _i2str_save = b;                              \
    if (_i2str_x < 0) {                                 \
      *_i2str_save++ = '-';                             \
      _i2str_x = -_i2str_x; }                           \
    auto _i2str_v = _i2str_save;                        \
    while (_i2str_x) {                                  \
      *_i2str_v++ = "0123456789"[_i2str_x % 10];        \
      _i2str_x /= 10;                                   \
    }                                                   \
    *_i2str_v = 0;                                      \
    _i2str_v--;                                         \
    auto _i2str_l = _i2str_save;                        \
    while (_i2str_l < _i2str_v) {                       \
      auto p = *_i2str_l;                               \
      *_i2str_l++ = *_i2str_v;                          \
      *_i2str_v-- = p;                                  \
    }                                                   \
  } while (0)

#define V_SN(C)                                 \
  do {                                          \
    static int __V_SN_sn = 0;                   \
    char __V_SN_buf[16];                        \
    __V_SN_sn++;                                \
    do { C } while (0);                         \
    _I2STR(__V_SN_sn, __V_SN_buf);              \
    write(1, __V_SN_buf, strlen(__V_SN_buf));   \
  } while (0)

#define V(x)                                    \
  do {                                          \
    auto __V_msg = #x " = ";                    \
    write(1, __V_msg, strlen(__V_msg));         \
    char __V_buf[16];                           \
    _I2STR((x), __V_buf);                       \
    write(1, __V_buf, strlen(__V_buf));         \
    char __V_n = '\n';                          \
    write(1, &__V_n, 1);                        \
  } while (0)

#define COUNT2(n, cmd) \
  do { \
    static int __COUNT2_sn = 0; \
    if (++__COUNT2_sn == (n)) { cmd } \
  } while (0)

#endif /* __log_h_ */
