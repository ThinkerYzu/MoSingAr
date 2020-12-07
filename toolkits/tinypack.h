/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __tinypack_h_
#define __tinypack_h_

/**
 * tinypack_value_trait of a type define how data of the type should
 * be pack and unpacked.  Whne a new type are added, a respective
 * tinypack_value_trait should be defined to handle data of the new
 * type.
 *
 * |tinypack| chains values together to pack all values into a
 * buffer. |tinypack<B, T>::field(V v)| add a new value of type V to
 * the chain, and return a new instance of type |tinypack<Base, V>|
 * where |Base| is |tinypack<B, T>|, the type of the instance of that
 * |field()| is called.  So, for a |tinypack| that pack 2 values of
 * type int and double, the type of the final instance will be
 * |tinypack<tinypack<tinypacker, int>, double>|.  |tinypacker| is
 * actually |tinypack<void, void>|, where all instance of |tinypack|
 * start with.
 *
 * |tinyunpack| is similar to |tinypack|, but it is for unpacking
 * data from a buffer.
 */

#include <utility>
#include <string.h>
#include <stdlib.h>

template <typename T>
class tinypack_value_trait {
public:
  static int size(T v) {
    return sizeof(T);
  }
  static void writebuf(T v, char* writeto) {
    memcpy(writeto, &v, sizeof(T));
  }
  static int rsize(T* v, const char* readfrom, unsigned int size) {
    if (size < sizeof(T)) {
      return -1;
    }
    return sizeof(T);
  }
  static void readbuf(T& v, const char* readfrom) {
    memcpy(&v, readfrom, sizeof(T));
  }
};

template<>
class tinypack_value_trait<const char*> {
public:
  static int size(const char*v) {
    return sizeof(unsigned int) + strlen(v) + 1;
  }
  static void writebuf(const char*v, char* writeto) {
    unsigned int sz = strlen(v) + 1;
    memcpy(writeto, &sz, sizeof(unsigned int));
    memcpy(writeto + sizeof(unsigned int), v, sz);
  }
  static int rsize(const char** v, const char* readfrom, unsigned int size) {
    if (size < sizeof(unsigned int)) {
      return -1;
    }
    unsigned int datasz;
    memcpy(&datasz, readfrom, sizeof(unsigned int));
    auto fullsize = datasz + sizeof(unsigned int);
    if (size < fullsize) {
      return -1;
    }
    return fullsize;
  }
  static int rsize(char** v, const char* readfrom, unsigned int size) {
    return rsize((const char**)v, readfrom, size);
  }
  static void readbuf(const char*&v, const char* readfrom) {
    unsigned int sz;
    memcpy(&sz, readfrom, sizeof(unsigned int));
    auto strbuf = (char*)malloc(sz);
    memcpy(strbuf, readfrom + sizeof(unsigned int), sz);
    v = strbuf;
  }
  static void readbuf(char*&v, const char* readfrom) {
    readbuf(*(const char**)&v, readfrom);
  }
};

struct fixedbuf {
  fixedbuf(char* buf, int sz)
    : buf(buf)
    , size(sz) {}
  fixedbuf(const fixedbuf& other)
    : buf(other.buf)
    , size(other.size) {}

  char* buf;
  int size;
};

template<>
class tinypack_value_trait<fixedbuf> {
public:
  static int size(const fixedbuf& v) {
    return v.size + sizeof(unsigned int);
  }
  static void writebuf(const fixedbuf& v, char* writeto) {
    memcpy(writeto, &v.size, sizeof(unsigned int));
    memcpy(writeto + sizeof(unsigned int), v.buf, v.size);
  }
  static int rsize(const fixedbuf* v, const char* readfrom, unsigned int size) {
    if (size < sizeof(unsigned int)) {
      return -1;
    }
    unsigned int rcvd_sz;
    memcpy(&rcvd_sz, readfrom, sizeof(unsigned int));
    if ((unsigned int)v->size != rcvd_sz) {
      return -1;
    }
    return rcvd_sz + sizeof(unsigned int);
  }
  static void readbuf(fixedbuf& v, const char* readfrom) {
    memcpy(v.buf, readfrom + sizeof(unsigned int), v.size);
  }
};

template<>
class tinypack_value_trait<char*> : public tinypack_value_trait<const char*> {};

template <typename Base, typename T>
class tinypack {
public:
  tinypack(const Base& base, T value)
    : base(base)
    , value(value) {}

  tinypack(const tinypack<Base, T>& other)
    : base(other.base)
    , value(other.value) {}

  template <typename V>
  tinypack<tinypack<Base, T>, V> field(V v) {
    return tinypack<tinypack<Base, T>, V>(*this, v);
  }
  int get_size() {
    return base.get_size() + value_size();
  }
  int value_size() {
    return tinypack_value_trait<T>::size(value);
  }

  char* pack() {
    auto buf = (char*)malloc(get_size());
    writebuf(buf);
    return buf;
  }

  int get_size_prefix() {
    return get_size() + sizeof(unsigned int);
  }

  char* pack_size_prefix() {
    auto sz = get_size();
    auto buf = (char*)malloc(sizeof(unsigned int) + sz);
    memcpy(buf, &sz, sizeof(unsigned int));
    writebuf(buf + sizeof(unsigned int));
    return buf;
  }

  void writebuf(char* buf) {
    auto writeto = buf + base.get_size();
    tinypack_value_trait<T>::writebuf(value, writeto);
    base.writebuf(buf);
  }

private:
  Base base;
  T value;
};

template <>
class tinypack<void, void> {
public:
  tinypack() {}
  tinypack(const tinypack<void, void>& other) {}
  template <typename V>
  tinypack<tinypack<void, void>, V> field(V v) {
    return tinypack<tinypack<void, void>, V>(*this, v);
  }
  int get_size() { return 0; }
  void writebuf(char* buf) {}
};

typedef tinypack<void, void> tinypacker;

template <typename Base, typename T>
class tinyunpack {
public:
  tinyunpack(const tinyunpack<Base, T>& other)
    : base(other.base)
    , value(other.value) {}

  tinyunpack(const Base& base, T& value)
    : base(base)
    , value(&value) {}

  int get_size(const char* buf, unsigned int bufsz) {
    auto basesz = base.get_size(buf, bufsz);
    if (basesz < 0) {
      return -1;
    }
    auto vsz = value_size(buf + basesz, bufsz - basesz);
    if (vsz < 0) {
      return -1;
    }
    auto sz = basesz + vsz;
    return sz;
  }
  int value_size(const char* buf, unsigned int bufsz) {
    return tinypack_value_trait<T>::rsize(value, buf, bufsz);
  }

  void unpack(const char* buf, unsigned int bufsz) {
    auto basesz = base.get_size(buf, bufsz);
    tinypack_value_trait<T>::readbuf(*value, buf + basesz);
    base.unpack(buf, bufsz);
  }

  const char* get_data() {
    return base.get_data();
  }
  unsigned int get_datasize() {
    return base.get_datasize();
  }

  template <typename V>
  tinyunpack<tinyunpack<Base, T>, V> field(V& v) {
    return tinyunpack<tinyunpack<Base, T>, V>(*this, v);
  }

  int get_size() {
    return get_size(get_data(), get_datasize());
  }
  bool check_completed() {
    return get_size() >= 0;
  }
  void unpack() {
    unpack(get_data(), get_datasize());
  }

private:
  Base base;
  T* value;
};

template<>
class tinyunpack<void, void> {
public:
  tinyunpack(const char* buf, unsigned int bufsz)
    : buf(buf)
    , bufsz(bufsz) {}

  tinyunpack(const tinyunpack<void, void>& other)
    : buf(other.buf)
    , bufsz(other.bufsz) {}

  template <typename V>
  tinyunpack<tinyunpack<void, void>, V> field(V& v) {
    return tinyunpack<tinyunpack<void, void>, V>(*this, v);
  }

  int get_size(const char* buf, unsigned int bufsz) { return 0; }

  void unpack(const char* buf, unsigned int bufsz) {
    return;
  }

  const char* get_data() { return buf; }

  unsigned int get_datasize() { return bufsz; }

  int get_size() {
    return 0;
  }

  bool check_completed() {
    return true;
  }

  void unpack() {}

private:
  const char* buf;
  unsigned int bufsz;
};

typedef tinyunpack<void, void> tinyunpacker;

#endif /* __tinypack_h_ */
