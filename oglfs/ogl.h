/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __ogl_h_
#define __ogl_h_

#include <stdint.h>
#include <map>
#include <memory>
#include <string>


namespace otypes {
struct object;
}

class ogl_file;
class ogl_dir;
class ogl_symlink;
class ogl_repo;

class ogl_entry {
public:
  enum ogl_type { OGL_NONE, OGL_REMOVED, OGL_NONEXISTENT, OGL_FILE, OGL_DIR, OGL_SYMLINK };

  ogl_entry(ogl_repo* repo) : repo(repo) {}

  virtual ogl_type get_type() { return OGL_NONE; }
  virtual ogl_file* to_file() { return nullptr; }
  virtual ogl_dir* to_dir() { return nullptr; }
  virtual ogl_symlink* to_symlink() { return nullptr; }

protected:
  ogl_repo* repo;
};

class ogl_removed : public ogl_entry {
public:
  ogl_removed(ogl_repo* repo) : ogl_entry(repo) {}
  virtual ogl_type get_type() { return OGL_REMOVED; }
};

class ogl_nonexistent : public ogl_entry {
public:
  ogl_nonexistent(ogl_repo* repo) : ogl_entry(repo) {}
  virtual ogl_type get_type() { return OGL_NONEXISTENT; }
};

class ogl_file : public ogl_entry {
public:
  ogl_file(ogl_repo* repo, const std::string& fname, const std::string& dirname)
    : ogl_entry(repo)
    , filename(fname)
    , dir(dirname)
    , mode(0)
    , own(false)
    , own_group(false)
    , is_new_file(true) {
  }
  virtual ogl_type get_type() { return OGL_FILE; }
  virtual ogl_file* to_file() { return this; }

  uint64_t hashcode() { return hash; }
  void set_hashcode(uint64_t hashcode) {
    hash = hashcode;
    is_new_file = false;
  }
  uint64_t get_mode() { return mode; }
  bool get_own() { return own; }
  bool get_own_group() { return own_group; }
  bool is_new() { return is_new_file; }

  int open();

  bool compute_hashcode();

private:
  uint64_t hash;
  const std::string filename;
  const std::string dir;
  uint16_t mode;
  bool own;
  bool own_group;
  bool is_new_file;
};

class ogl_dir : public ogl_entry {
public:
  using entries_t = std::map<std::string, std::unique_ptr<ogl_entry>>;
  using iterator = entries_t::iterator;

  ogl_dir(ogl_repo* repo, ogl_dir* parent, const std::string& dirname)
    : ogl_entry(repo)
    , parent(parent)
    , dirname(dirname)
    , abspath(parent ? parent->abspath + "/" + dirname : dirname)
    , mode(0)
    , own(false)
    , own_group(false)
    , modified(false)
    , loaded(false) {
  }
  virtual ogl_type get_type() { return OGL_DIR; }
  virtual ogl_dir* to_dir() { return this; }

  uint64_t hashcode() { return hash; }
  void set_hashcode(uint64_t hashcode) { hash = hashcode; }
  bool dump();
  bool load();

  bool add_file(const std::string &filename);
  bool add_dir(const std::string &dirname);
  bool add_symlink(const std::string& target);
  bool remove(const std::string &name);
  bool mark_nonexistent(const std::string &name);

  bool has_modified() { return modified; }
  bool has_loaded() { return loaded; }

  ogl_entry* lookup(const std::string &name);

  uint64_t get_mode() { return mode; }
  bool get_own() { return own; }
  bool get_own_group() { return own_group; }

  iterator begin() {
    return entries.begin();
  }

  iterator end() {
    return entries.end();
  }

private:
  bool in_memory(const std::string &name) {
    return entries.find(name) != entries.end();
  }

  void mark_modified() {
    for (auto pd = this; pd; pd = pd->parent) {
      pd->modified = true;
    }
  }

  ogl_dir* parent;
  uint64_t hash;
  const std::string dirname;
  const std::string abspath;
  entries_t entries;
  uint16_t mode;
  bool own;
  bool own_group;
  bool modified;
  bool loaded;
};

class ogl_symlink : public ogl_entry {
public:
  ogl_symlink(ogl_repo* repo)
    : ogl_entry(repo)
    , mode(0)
    , own(false)
    , own_group(false)
    , loaded(false) {
  }
  virtual ogl_type get_type() { return OGL_SYMLINK; }
  virtual ogl_symlink* to_symlink() {
    return this;
  }

  const std::string& get_target() { return target; }

  uint64_t hashcode() { return hash; }
  bool set_hashcode(uint64_t hashcode) { hash = hashcode; }

  uint64_t get_mode() { return mode; }
  bool get_own() { return own; }
  bool get_own_group() { return own_group; }

  bool load();
  bool dump();
  bool has_loaded() { return loaded; }

private:
  const std::string target;
  uint64_t hash;
  uint16_t mode;
  bool own;
  bool own_group;
  bool loaded;
};

class ogl_repo {
public:
  ogl_repo(const std::string &root, const std::string &repo);
  ~ogl_repo();

  static bool init(const std::string &repo);

  const std::string& get_rootpath() {
    return root_path;
  }

  ogl_dir* get_root() {
    return root_dir.get();
  }

  bool add_file(const std::string &path);
  bool add_dir(const std::string &path);
  bool remove(const std::string &path);
  bool mark_nonexistent(const std::string &path);
  ogl_entry* find(const std::string &path);

  bool commit();

  bool store_obj(uint64_t hash, const otypes::object* obj);
  std::unique_ptr<otypes::object> load_obj(uint64_t hash);

private:
  bool update_root_ref();

  const std::string root_path;
  const std::string repo_path;
  std::unique_ptr<ogl_dir> root_dir;
};

#endif /* __ogl_h_ */
