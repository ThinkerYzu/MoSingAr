/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __otypes_h_
#define __otypes_h_

#include <stdint.h>
#include <cassert>

namespace otypes {

struct object {
  constexpr static uint16_t MAGIC = 0x091f;
  uint16_t magic;
  uint16_t type;
  uint16_t size;

  enum types {
    INVALID,
    DIR,
    SUPER_DIR,
    SYMLINK,
  };
};

struct dentry {
  /*
   * Since dir_objects & superdir_objects don't always keep the whole
   * list of file names of an directory, the ENT_NONEXISTENT type is
   * necessary to remember the filenames that are not existing in a
   * directory in case of being checked frequently.
   */
  enum entry_type {
    ENT_NONEXISTENT,
    ENT_FILE,
    ENT_DIR,
    ENT_SYMLINK,
  };
  constexpr static uint16_t USER_MASK = 01000;
  constexpr static uint16_t GROUP_MASK = 02000;
  constexpr static uint16_t PLACEHOLDER_MASK = 04000;
  uint16_t mode;
  uint16_t name_offset;
  uint32_t tm;

  entry_type get_entry_type() {
    return (entry_type)(mode >> 12);
  }
  bool same_user() {
    return mode & USER_MASK;
  }
  bool same_group() {
    return mode & GROUP_MASK;
  }
  bool is_placeholder() {
    return mode & PLACEHOLDER_MASK;
  }
  uint16_t perms() {
    return mode & 0777;
  }
};

struct dir_object : public object {
  uint16_t ent_num;
  uint16_t hash_offset;
  uint16_t str_offset;
  dentry entries[];

  const char* get_name(uint16_t endx) {
    assert(endx < ent_num);
    auto entry = &entries[endx];
    return (char*)this + entry->name_offset;
  }
  uint64_t get_hash(uint16_t endx) {
    assert(endx < ent_num);
    auto hashcodes = (uint64_t*)((char*)this + hash_offset);
    return hashcodes[endx];
  }
};

struct superdir_object : public object {
  uint16_t ent_num;
  uint32_t dent_num;
  uint64_t hashcodes[];

  uint64_t get_hash(uint16_t ndx) {
    assert(ndx < ent_num);
    return hashcodes[ndx];
  }
  uint16_t get_entry_num_dir(uint16_t ndx) {
    assert(ndx < ent_num);
    return ((uint16_t*)&hashcodes[ent_num])[ndx];
  }
  const char* get_first_name(uint16_t ndx) {
    assert(ndx < ent_num);
    auto offsets = (uint16_t*)&hashcodes[ent_num];
    return (char*)this + offsets[ndx];
  }
};

struct symlink_object : public object {
  int linkto_size;
  char linkto[];
};

} // otypes

#endif /* __otypes_h_ */
