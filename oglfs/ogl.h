/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#ifndef __ogl_h_
#define __ogl_h_

#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <functional>

extern "C" {
struct stat;
};

namespace otypes {
struct object;
}

class ogl_file;
class ogl_dir;
class ogl_symlink;
class ogl_repo;
class ogl_nonexistent;
class ogl_local;

class ogl_entry {
public:
  /**
   * OGL_LOCAL marks a file or a directory local.  It means it should
   * be got from local the file system.
   */
  enum ogl_type { OGL_NONE, OGL_REMOVED, OGL_NONEXISTENT, OGL_FILE, OGL_DIR, OGL_SYMLINK, OGL_LOCAL };

  ogl_entry(ogl_repo* repo) : repo(repo) {}

  virtual ogl_type get_type() const { return OGL_NONE; }
  virtual ogl_file* to_file() { return nullptr; }
  virtual ogl_dir* to_dir() { return nullptr; }
  virtual ogl_symlink* to_symlink() { return nullptr; }
  virtual ogl_nonexistent* to_nonexistent() { return nullptr; }
  virtual ogl_local* to_local() { return nullptr; }
  virtual std::unique_ptr<ogl_entry> clone() { return nullptr; }

protected:
  ogl_repo* repo;
};

class ogl_removed : public ogl_entry {
public:
  ogl_removed(ogl_repo* repo) : ogl_entry(repo) {}
  virtual ogl_type get_type() const { return OGL_REMOVED; }
  virtual std::unique_ptr<ogl_entry> clone() { return std::make_unique<ogl_removed>(repo); }
};

class ogl_nonexistent : public ogl_entry {
public:
  ogl_nonexistent(ogl_repo* repo) : ogl_entry(repo) {}
  virtual ogl_type get_type() const { return OGL_NONEXISTENT; }
  virtual ogl_nonexistent* to_nonexistent() { return this; }
  virtual std::unique_ptr<ogl_entry> clone() { return std::make_unique<ogl_nonexistent>(repo); }
};

class ogl_local : public ogl_entry {
public:
  ogl_local(ogl_repo* repo) : ogl_entry(repo) {}
  virtual ogl_type get_type() const { return OGL_LOCAL; }
  virtual ogl_local* to_local() { return this; }
  virtual std::unique_ptr<ogl_entry> clone() { return std::make_unique<ogl_local>(repo); }
};


/**
 * The instances of ogl_file will not dump them-self to repositories.
 * Instead, they compute hash codes of files behind it.
 *
 * The objects are started with an invalid hash code.  The valid hash
 * code of an instance will be computed when commit it.
 */
class ogl_file : public ogl_entry {
public:
  ogl_file(ogl_repo* repo, ogl_dir* dir, const std::string& fname)
    : ogl_entry(repo)
    , dir(dir)
    , filename(fname)
    , mode(0)
    , own(false)
    , own_group(false)
    , valid_hash(false) {
  }
  virtual ogl_type get_type() const { return OGL_FILE; }
  virtual ogl_file* to_file() { return this; }
  virtual std::unique_ptr<ogl_entry> clone() {
    auto file = std::make_unique<ogl_file>(repo, dir, filename);
    file->hash  = hash;
    file->mode = mode;
    file->own = own;
    file->own_group = own_group;
    file->valid_hash = valid_hash;
    return file;
  }

  uint64_t hashcode() { return hash; }
  void set_hashcode(uint64_t hashcode) {
    hash = hashcode;
    valid_hash = true;
  }
  uint64_t get_mode() { return mode; }
  bool get_own() { return own; }
  bool get_own_group() { return own_group; }
  bool has_valid_hash() { return valid_hash; }

  int open();

  bool compute_hashcode();

private:
  ogl_dir* dir;
  uint64_t hash;
  const std::string filename;
  uint16_t mode;
  bool own;
  bool own_group;
  bool valid_hash;
};

/**
 * ogl_dir starts as unmodified and unloaded.  It becomes unmodified and
 * loaded once it is loaded or dumped.
 *
 * Whenever an instance of ogl_dir is modified, |loaded| is
 * meaningless.  |loaded| is meaningful only if |modified| is false.
 * When |modified| and |loaded| are both false, it means the content
 * of the instance should be loaded from the storage before using the
 * instance.
 */
class ogl_dir : public ogl_entry {
public:
  using entries_t = std::map<const std::string, std::unique_ptr<ogl_entry>>;
  using iterator = entries_t::iterator;
  enum diff_ops { DIFF_ADD, DIFF_RM, DIFF_MOD };
  using diff_handler = std::function<bool(diff_ops op, const ogl_dir* parent, const ogl_dir* oparent, const std::string& name)>;

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
  virtual ogl_type get_type() const { return OGL_DIR; }
  virtual ogl_dir* to_dir() { return this; }
  virtual std::unique_ptr<ogl_entry> clone() {
    auto dir = std::make_unique<ogl_dir>(repo, parent, dirname);
    copy_to(dir.get());
    return dir;
  }

  uint64_t hashcode() { return hash; }
  void set_hashcode(uint64_t hashcode) { hash = hashcode; }
  bool dump();
  bool load();

  bool add_file(const std::string &filename);
  bool add_dir(const std::string &dirname);
  bool add_symlink(const std::string& filename);
  bool mark_local(const std::string& filename);
  bool remove(const std::string &name);
  // This is used to remember that a file is not existing on the file
  // system, so we don't need to check it every time.
  bool mark_nonexistent(const std::string &name);

  bool has_modified() { return modified; }
  bool has_loaded() { return loaded; }

  ogl_entry* lookup(const std::string &name) const;

  uint64_t get_mode() { return mode; }
  bool get_own() { return own; }
  bool get_own_group() { return own_group; }

  iterator begin() {
    return entries.begin();
  }

  iterator end() {
    return entries.end();
  }

  const std::string get_path(const std::string &name = "") const {
    if (name.size() == 0) {
      return abspath;
    }
    return abspath + "/" + name;
  }

  /**
   * Get stat of the name in the directory from the backing file
   * system.
   */
  bool get_stat( struct stat& buf, const std::string& name = "");

  /**
   * Find out the differences from |other|, and call |handler| to work
   * on them.
   */
  void diff(const ogl_dir* other, const diff_handler& handler) const;

  void copy_to(ogl_dir* dst) const;

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

/**
 * The instances of ogl_symlink are started as modified and not loaded.
 * One being dumped or loaded, they are loaded and not modified.
 *
 * When a symlink is modified, it should be dumped for commiting later.
 */
class ogl_symlink : public ogl_entry {
public:
  constexpr static int MAX_TARGET_SIZE = 256;

  ogl_symlink(ogl_repo* repo, ogl_dir* dir, const std::string& name)
    : ogl_entry(repo)
    , dir(dir)
    , name(name)
    , mode(0)
    , own(false)
    , own_group(false)
    , modified(true)
    , loaded(false) {
  }
  virtual ogl_type get_type() const { return OGL_SYMLINK; }
  virtual ogl_symlink* to_symlink() {
    return this;
  }
  virtual std::unique_ptr<ogl_entry> clone() {
    auto link = std::make_unique<ogl_symlink>(repo, dir, name);
    link->hash = hash;
    link->mode = mode;
    link->own = own;
    link->own_group = own_group;
    return link;
  }

  const std::string& get_target() { return target; }

  uint64_t hashcode() { return hash; }
  void set_hashcode(uint64_t hashcode) {
    hash = hashcode;
    modified = false;
  }

  uint64_t get_mode() { return mode; }
  bool get_own() { return own; }
  bool get_own_group() { return own_group; }

  bool load();
  bool dump();
  bool has_modified() { return modified; }
  bool has_loaded() { return loaded; }

  void mark_modified() {
    modified = true;
  }

private:
  ogl_dir* dir;
  std::string name;
  std::string target;
  uint64_t hash;
  uint16_t mode;
  bool own;
  bool own_group;
  bool modified;
  bool loaded;
};

class ogl_repo {
public:
  ogl_repo(const std::string &root, const std::string &repo);
  ogl_repo(const std::string &root, const std::string &repo, uint64_t root_hash);
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
  bool add_symlink(const std::string &path);
  bool mark_local(const std::string& filename);
  bool remove(const std::string &path);
  bool mark_nonexistent(const std::string &path);
  ogl_entry* find(const std::string &path);
  ogl_dir* find_dir(const std::string &path) {
    auto ent = find(path);
    if (ent == nullptr) {
      return nullptr;
    }
    return ent->to_dir();
  }

  bool commit();

  bool store_obj(uint64_t hash, const otypes::object* obj);
  std::unique_ptr<otypes::object> load_obj(uint64_t hash);

  /**
   * Merge the changes made by |src| from |common| onto |dst|.
   *
   * \return true for success, and false for conflictions.
   */
  static
  bool merge(ogl_repo* src, ogl_repo* dst, ogl_repo* common);

private:
  bool update_root_ref();
  ogl_dir* get_parent_dir(const std::string &path, std::string &basename);

  const std::string root_path;
  const std::string repo_path;
  std::unique_ptr<ogl_dir> root_dir;
};

#endif /* __ogl_h_ */
