/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __tinypack_h_
#define __tinypack_h_

#include <utility>
#include <string.h>

template <typename T>
class tinypack_value_trait {
public:
  static int size(T v) {
    return sizeof(T);
  }
  static void writebuf(T v, char* writeto) {
    memcpy(writeto, &v, sizeof(T));
  }
  static int rsize(const char* readfrom, unsigned int size) {
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
    memcpy(writeto + sizeof(unsigned int), v, size(v));
  }
  static int rsize(const char* readfrom, unsigned int size) {
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
  static void readbuf(const char*&v, const char* readfrom) {
    unsigned int sz;
    memcpy(&sz, readfrom, sizeof(unsigned int));
    auto strbuf = new char[sz];
    memcpy(strbuf, readfrom + sizeof(unsigned int), sz);
    v = strbuf;
  }
};

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

  char*pack() {
    auto buf = new char[get_size()];
    writebuf(buf);
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

class tinypacker : public tinypack<void, void> {
};

#if 0
template <typename Base, typename T>
class tinyunpack {
public:
  tinyunpack(const Base& base, T& value)
    : base(base)
    , value(&value) {}

  tinyunpack(const tinyunpack<Base, T>& other)
    : base(other.base)
    , value(other.value) {}

  template <typename V>
  tinyunpack<tinyunpack<Base, T>, V> field(V& v) {
    return tinyunpack<tinyunpack<Base, T>, V>(*this, v);
  }

  int get_size() {
    return base.get_size() + value_size();
  }
  int value_size() {
    return tinypack_value_trait<T>::rsize(value);
  }

private:
  Base base;
  T* value;
};
#endif

template <typename Base, typename T>
class tinyunpack_nobuf {
public:
  tinyunpack_nobuf(const tinyunpack_nobuf<Base, T>& other)
    : base(other.base)
    , value(other.value) {}

  tinyunpack_nobuf(const Base& base, T& value)
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
    return tinypack_value_trait<T>::rsize(buf, bufsz);
  }

  void unpack(const char* buf, unsigned int bufsz) {
    auto basesz = base.get_size(buf, bufsz);
    tinypack_value_trait<T>::readbuf(*value, buf + basesz);
    base.unpack(buf, bufsz);
  }

private:
  Base base;
  T* value;
};

template <typename Base, typename T>
class tinyunpack : public tinyunpack_nobuf<Base, T> {
public:
  tinyunpack(const Base& base, T& value, const char* buf, unsigned int bufsz)
    : tinyunpack_nobuf<Base, T>(base, value)
    , buf(buf)
    , bufsz(bufsz) {}

  template <typename V>
  tinyunpack<tinyunpack_nobuf<Base, T>, V> field(V& v) {
    return tinyunpack<tinyunpack_nobuf<Base, T>, V>(*this, v, buf, bufsz);
  }

  int get_size() {
    return tinyunpack_nobuf<Base, T>::get_size(buf, bufsz);
  }
  bool check_completed() {
    return get_size() >= 0;
  }
  void unpack() {
    tinyunpack_nobuf<Base, T>::unpack(buf, bufsz);
  }

private:
  const char* buf;
  unsigned int bufsz;
};

template<>
class tinyunpack_nobuf<void, void> {
public:
  tinyunpack_nobuf() {}
  tinyunpack_nobuf(const tinyunpack<void, void>& other) {}

  template <typename V>
  tinyunpack<tinyunpack<void, void>, V> field(V& v) {
    return tinyunpack<tinyunpack<void, void>, V>(*this, v);
  }

  int get_size(const char* buf, unsigned int bufsz) { return 0; }

  void unpack(const char* buf, unsigned int bufsz) {
    return;
  }
};

template<>
class tinyunpack<void, void> : public tinyunpack_nobuf<void, void> {
public:
  tinyunpack(const char* buf, unsigned int bufsz)
    : buf(buf)
    , bufsz(bufsz) {}

  template <typename V>
  tinyunpack<tinyunpack_nobuf<void, void>, V> field(V& v) {
    return tinyunpack<tinyunpack_nobuf<void, void>, V>(*this, v, buf, bufsz);
  }

  int get_size() {
    return 0;
  }
  bool check_completion() {
    return true;
  }

  void unpack() {}

private:
  const char* buf;
  unsigned int bufsz;
};

class tinyunpacker : public tinyunpack<void, void> {
public:
  tinyunpacker(const char* buf, unsigned int bufsz)
    : tinyunpack(buf, bufsz) {}
};

#endif /* __tinypack_h_ */
