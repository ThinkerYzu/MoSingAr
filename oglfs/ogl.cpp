/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/**
 * The directory of a repository looks like
 * - root_path/
 *   - root-ref
 *     - contains the hash code of the current root directory object.
 *   - objects/
 *     - object files named with their 64-bits hash code.
 */
#include "ogl.h"
#include "otypes.h"

#include "errhandle.h"

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <vector>
#include <algorithm>

#include "sha2.h"

#define BUF_SZ (4096 * 8)

static uint64_t
compute_hash_buf(void* buf, int size) {
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  SHA256_Update(&ctx, (const uint8_t*)buf, size);
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256_Final(digest, &ctx);
  uint64_t hash = 0;
  for (int i = 0; i < 8; i++) {
    hash = (hash << 8) | (uint64_t)digest[i];
  }
  return hash;
}

int
ogl_file::open() {
  auto path = dir->get_path(filename);
  auto fd = ::open(path.c_str(), O_RDONLY);
  return fd;
}

bool
ogl_file::compute_hashcode() {
  auto fd = open();
  if (fd < 0) {
    return false;
  }

  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  int cp;
  char* buf = new char[BUF_SZ];
  assert(buf);
  while ((cp = read(fd, buf, BUF_SZ)) > 0) {
    SHA256_Update(&ctx, (const uint8_t*)buf, cp);
  }
  delete buf;

  close(fd);

  if (cp < 0) {
    return false;
  }

  // Get the hash code
  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256_Final(digest, &ctx);
  uint64_t hash = 0;
  for (int i = 0; i < 8; i++) {
    hash = (hash << 8) | (uint64_t)digest[i];
  }
  set_hashcode(hash);

  // Collects perms, that will be read and dump by the containing dir.
  struct stat statbuf;
  auto ok = dir->get_stat(statbuf, filename);
  if (!ok) {
    return false;
  }
  own = getuid() == statbuf.st_uid;
  own_group = getgid() == statbuf.st_gid;
  mode = statbuf.st_mode & 0777;

  return true;
}

void
ogl_file::mark_modified() {
  valid_hash = false;
  dir->mark_modified();
}

bool
ogl_dir::add_file(const std::string& filename) {
  if (lookup(filename) != nullptr) {
    return false;
  }
  entries[filename] = std::make_unique<ogl_file>(repo, this, filename);
  mark_modified();
  return true;
}

bool
ogl_dir::add_dir(const std::string& dirname) {
  if (lookup(dirname) != nullptr) {
    return false;
  }
  std::unique_ptr<ogl_dir> dir = std::make_unique<ogl_dir>(repo, this, dirname);
  // A new created ogl_dir should be loaded and modified, so it will
  // be dumped to commit the repo later.
  dir->loaded = true;
  dir->mark_modified();
  entries[dirname] = std::move(dir);
  return true;
}

bool
ogl_dir::add_symlink(const std::string& filename) {
  if (lookup(filename) != nullptr) {
    return false;
  }
  std::unique_ptr<ogl_symlink> sym = std::make_unique<ogl_symlink>(repo, this, filename);
  entries[filename] = std::move(sym);
  mark_modified();
  return true;
}

bool
ogl_dir::remove(const std::string& filename) {
  auto ent = lookup(filename);
  if (ent == nullptr) {
    return false;
  }

  entries.erase(filename);
  mark_modified();
  return true;
}

bool
ogl_dir::mark_local(const std::string& filename) {
  auto ent = lookup(filename);
  if (ent != nullptr) {
    return false;
  }

  std::unique_ptr<ogl_local> local =
    std::make_unique<ogl_local>(repo);
  entries[filename] = std::move(local);
  mark_modified();
  return true;
}

bool
ogl_dir::mark_nonexistent(const std::string& filename) {
  auto ent = lookup(filename);
  if (ent != nullptr) {
    return false;
  }

  std::unique_ptr<ogl_nonexistent> nonexistent =
    std::make_unique<ogl_nonexistent>(repo);
  entries[filename] = std::move(nonexistent);
  mark_modified();
  return true;
}

bool
ogl_dir::dump() {
  assert(loaded);

  // Collect files in the directory, and sort them.
  std::vector<std::string> names;
  int cnt_nonexistent = 0;
  int cnt_file = 0;
  int cnt_dir = 0;
  int cnt_symlink = 0;
  int cnt_local = 0;
  int str_total = 0;
  for (auto itr = entries.begin();
       itr != entries.end();
       ++itr) {
    switch (itr->second->get_type()) {
    case OGL_NONEXISTENT:
      cnt_nonexistent++;
      str_total += itr->first.size() + 1;
      names.push_back(itr->first);
      break;

    case OGL_FILE:
      cnt_file++;
      str_total += itr->first.size() + 1;
      names.push_back(itr->first);
      break;

    case OGL_DIR:
      cnt_dir++;
      str_total += itr->first.size() + 1;
      names.push_back(itr->first);
      break;

    case OGL_SYMLINK:
      cnt_symlink++;
      str_total += itr->first.size() + 1;
      names.push_back(itr->first);
      break;

    case OGL_LOCAL:
      cnt_local++;
      str_total += itr->first.size() + 1;
      names.push_back(itr->first);
      break;
    }
  }
  std::sort(names.begin(), names.end());

  int objsize = sizeof(otypes::dir_object);
  objsize += sizeof(otypes::dentry) * names.size();
  int hash_offset = objsize;
  objsize += sizeof(uint64_t) * names.size();
  int str_offset = objsize;
  objsize += str_total;


  auto obj = reinterpret_cast<otypes::dir_object*>(new char[objsize]);
  bzero(obj, objsize);
  obj->magic = otypes::object::MAGIC;
  obj->type = otypes::object::DIR;
  obj->size = objsize;

  obj->ent_num = names.size();
  obj->hash_offset = hash_offset;
  obj->str_offset = str_offset;
  auto hashcodes = reinterpret_cast<uint64_t*>(reinterpret_cast<char*>(obj) +
                                               hash_offset);
  auto strptr = reinterpret_cast<char*>(obj) + str_offset;

  // Fill entries of otypes::dir_object
  int ndx = 0;
  for (auto name = names.begin();
       name != names.end();
       ++name, ++ndx) {
    auto entry = lookup(*name);
    auto entobj = &obj->entries[ndx];

    entobj->name_offset = strptr - reinterpret_cast<char*>(obj);
    memcpy(strptr, name->c_str(), name->size() + 1);
    strptr += name->size() + 1;

    switch (entry->get_type()) {
    case OGL_NONEXISTENT:
      {
        entobj->mode = otypes::dentry::ENT_NONEXISTENT << 12;
        hashcodes[ndx] = 0;
      }
      break;

    case OGL_FILE:
      {
        auto file = entry->to_file();
        assert(file);
        entobj->mode = file->get_mode() | (otypes::dentry::ENT_FILE << 12);
        if (file->get_own()) {
          entobj->mode |= otypes::dentry::USER_MASK;
        }
        if (file->get_own_group()) {
          entobj->mode |= otypes::dentry::GROUP_MASK;
        }
        hashcodes[ndx] = file->hashcode();
      }
      break;

    case OGL_DIR:
      {
        auto dir = entry->to_dir();
        assert(dir);
        entobj->mode = dir->get_mode() | (otypes::dentry::ENT_DIR << 12);
        if (dir->get_own()) {
          entobj->mode |= otypes::dentry::USER_MASK;
        }
        if (dir->get_own_group()) {
          entobj->mode |= otypes::dentry::GROUP_MASK;
        }
        hashcodes[ndx] = dir->hashcode();
      }
      break;

    case OGL_SYMLINK:
      {
        auto symlink = entry->to_symlink();
        assert(symlink);
        entobj->mode = symlink->get_mode()  | (otypes::dentry::ENT_SYMLINK << 12);
        if (symlink->get_own()) {
          entobj->mode |= otypes::dentry::USER_MASK;
        }
        if (symlink->get_own_group()) {
          entobj->mode |= otypes::dentry::GROUP_MASK;
        }
        hashcodes[ndx] = symlink->hashcode();
      }
      break;

    case OGL_LOCAL:
      {
        entobj->mode = otypes::dentry::ENT_LOCAL << 12;
        hashcodes[ndx] = 0;
      }
      break;
    }
  }

  auto hash = compute_hash_buf(obj, objsize);
  auto ok = repo->store_obj(hash, obj);
  set_hashcode(hash);

  // Collects perms, that will be read and dump by the containing dir.
  struct stat statbuf;
  ok = get_stat(statbuf);
  if (!ok) {
    return false;
  }
  own = getuid() == statbuf.st_uid;
  own_group = getgid() == statbuf.st_gid;
  mode = statbuf.st_mode & 0777;

  modified = false;
  loaded = true;

  return ok;
}

bool
ogl_dir::load() {
  std::unique_ptr<otypes::object> obj = std::move(repo->load_obj(hash));
  assert(obj->type == otypes::object::DIR);
  std::unique_ptr<otypes::dir_object> dirobj(reinterpret_cast<otypes::dir_object*>(obj.release()));
  for (auto i = 0; i < dirobj->ent_num; i++) {
    auto entry = &dirobj->entries[i];
    auto type = (entry->mode >> 12) & 0xf;
    std::string name(dirobj->get_name(i));
    auto hash = dirobj->get_hash(i);
    switch (type) {
    case otypes::dentry::ENT_NONEXISTENT:
      {
        std::unique_ptr<ogl_nonexistent> nonexistent =
          std::make_unique<ogl_nonexistent>(repo);
        entries[name] = std::move(nonexistent);
      }
      break;

    case otypes::dentry::ENT_FILE:
      {
        std::unique_ptr<ogl_file> file =
          std::make_unique<ogl_file>(repo, this, name);
        file->set_hashcode(hash);
        entries[name] = std::move(file);
      }
      break;

    case otypes::dentry::ENT_DIR:
      {
        std::unique_ptr<ogl_dir> dir =
          std::make_unique<ogl_dir>(repo, this, name);
        dir->set_hashcode(hash);
        entries[name] = std::move(dir);
      }
      break;

    case otypes::dentry::ENT_SYMLINK:
      {
        std::unique_ptr<ogl_symlink> symlink =
          std::make_unique<ogl_symlink>(repo, this, name);
        symlink->set_hashcode(hash);
        entries[name] = std::move(symlink);
      }
      break;

    case otypes::dentry::ENT_LOCAL:
      {
        std::unique_ptr<ogl_local> local =
          std::make_unique<ogl_local>(repo);
        entries[name] = std::move(local);
      }
      break;

    default:
      ABORT("Unknown object type");
    }
  }

  modified = false;
  loaded = true;

  return true;
}

ogl_entry*
ogl_dir::lookup(const std::string& name) const {
  auto itr = entries.find(name);
  if (itr == entries.end()) {
    return nullptr;
  }
  return itr->second.get();
}

/**
 * Return the |stat| of a file, a sub-directory, or a link in the directory.
 */
bool
ogl_dir::get_stat(struct stat& buf, const std::string& name) {
  auto path = get_path(name);
  int r = lstat(path.c_str(), &buf);
  return r == 0;
}

ogl_repo::ogl_repo(const std::string &root, const std::string &repo)
  : root_path(root)
  , repo_path(repo) {
  auto _root_ref = repo_path + "/root-ref";
  auto root_ref = _root_ref.c_str();
  auto fd = open(root_ref, O_RDONLY);
  assert(fd >= 0);
  char buf[17];
  auto cp = read(fd, buf, 17);
  assert(cp == 17 && buf[16] == '\n');
  buf[16] = 0;
  uint64_t hash = std::stoul(buf, nullptr, 16);
  root_dir = std::make_unique<ogl_dir>(this, nullptr, root);
  root_dir->set_hashcode(hash);
  bool ok = root_dir->load();
  assert(ok);
}

ogl_repo::ogl_repo(const std::string &root, const std::string &repo, uint64_t root_hash)
  : root_path(root)
  , repo_path(repo) {
  root_dir = std::make_unique<ogl_dir>(this, nullptr, root);
  root_dir->set_hashcode(root_hash);
  bool ok = root_dir->load();
  assert(ok);
}

bool
ogl_symlink::dump() {
  char target_cstr[MAX_TARGET_SIZE];
  auto fullpath = dir->get_path() + "/" + name;
  auto cp = readlink(fullpath.c_str(), target_cstr, sizeof(target_cstr));
  assert(cp > 0 && cp < MAX_TARGET_SIZE);
  target_cstr[cp] = 0;
  target = target_cstr;

  auto sz = sizeof(otypes::symlink_object) + target.size() + 1;
  auto buf = new char[sz];
  auto obj = new(buf) otypes::symlink_object;
  obj->magic = otypes::object::MAGIC;
  obj->type = otypes::object::SYMLINK;
  obj->size = sz;
  obj->linkto_size = target.size() + 1;
  memcpy(obj->linkto, target.c_str(), target.size() + 1);
  auto hash = compute_hash_buf(buf, sz);

  auto ok = repo->store_obj(hash, obj);

  if (ok) {
    set_hashcode(hash);
    loaded = true;
  }

  delete buf;

  if (!ok) {
    return false;
  }

  // Collects perms, that will be read and dump by the containing dir.
  struct stat statbuf;
  ok = dir->get_stat(statbuf, name);
  if (!ok) {
    return false;
  }
  own = getuid() == statbuf.st_uid;
  own_group = getgid() == statbuf.st_gid;
  mode = statbuf.st_mode & 0777;

  return true;
}

bool
ogl_symlink::load() {
  std::unique_ptr<otypes::object> obj = repo->load_obj(hashcode());
  if (!obj) {
    return false;
  }
  assert(obj->type == otypes::object::SYMLINK);
  auto slobj = reinterpret_cast<otypes::symlink_object*>(obj.get());
  assert(slobj->linkto[slobj->linkto_size - 1] == 0);
  target = slobj->linkto;
  return true;
}

ogl_repo::~ogl_repo() {
}

static bool
write_root_ref(const std::string& repo_path, uint64_t hash) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%016lx", hash);

  std::string rootref = repo_path + "/root-ref";
  auto fd = open(rootref.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    return false;
  }

  auto cp = write(fd, buf, strlen(buf));
  if (cp != strlen(buf)) {
    close(fd);
    return false;
  }
  cp = write(fd, "\n", 1);
  close(fd);
  if (cp != 1) {
    return false;
  }
  return true;
}

bool
ogl_repo::init(const std::string& repo) {
  std::unique_ptr<otypes::dir_object> rootobj = std::make_unique<otypes::dir_object>();
  rootobj->magic = otypes::object::MAGIC;
  rootobj->type = otypes::object::DIR;
  rootobj->size = sizeof(otypes::dir_object);
  rootobj->ent_num = 0;
  rootobj->hash_offset = sizeof(otypes::dir_object);
  rootobj->str_offset = sizeof(otypes::dir_object);
  auto hash = compute_hash_buf(rootobj.get(), sizeof(otypes::dir_object));
  auto r = mkdir(repo.c_str(), 0755);
  if (r < 0) {
    return false;
  }
  std::string objs = repo + "/objects";
  r = mkdir(objs.c_str(), 0755);
  if (r < 0) {
    return false;
  }
  char buf[24];
  snprintf(buf, sizeof(buf), "%016lx", hash);
  std::string objfilename = objs + "/" + buf;
  auto fd = open(objfilename.c_str(), O_WRONLY | O_CREAT, 0644);
  if (fd < 0) {
    return false;
  }
  auto cp = write(fd, rootobj.get(), sizeof(otypes::dir_object));
  close(fd);
  if (cp != sizeof(otypes::dir_object)) {
    return false;
  }

  auto ok = write_root_ref(repo, hash);
  return ok;
}

bool
ogl_repo::commit() {
  if (!root_dir->has_modified()) {
    return true;
  }
  std::vector<ogl_entry*> todumps;
  std::vector<ogl_dir*> dirs;
  todumps.push_back(root_dir.get());
  while (todumps.size()) {
    auto ent = todumps.back();
    todumps.pop_back();

    switch (ent->get_type()) {
    case ogl_entry::OGL_NONEXISTENT:
      break;

    case ogl_entry::OGL_FILE:
      {
        auto file = ent->to_file();
        if (!file->has_valid_hash()) {
          auto ok = file->compute_hashcode();
          if (!ok) {
            return false;
          }
        }
      }
      break;

    case ogl_entry::OGL_DIR:
      {
        auto dir = ent->to_dir();
        if (!dir->has_modified()) {
          continue;
        }
        // All modified ogl_dir should be loaded.
        assert(dir->has_loaded());
        dirs.push_back(dir);

        for (auto chent = dir->begin(); chent != dir->end(); ++chent) {
          todumps.push_back(chent->second.get());
        }
      }
      break;

    case ogl_entry::OGL_SYMLINK:
      {
        auto link = ent->to_symlink();
        if (link->has_modified()) {
          auto ok = link->dump();
          if (!ok) {
            return false;
          }
        }
      }
      break;

    default:
      break;
    }
  }

  for (auto diritr = dirs.rbegin(); diritr != dirs.rend(); ++diritr) {
    auto ok = (*diritr)->dump();
    if (!ok) {
      return false;
    }
  }

  auto ok = update_root_ref();

  return true;
}

static bool
ancest_of_path(const std::string& ancest, const std::string& descent) {
  assert(ancest.size() > 0 && ancest.front() == '/');
  assert(descent.size() > 0 && descent.front() == '/');
  if (ancest == "/") {
    return descent.size() > 1;
  }
  // For not root dir of the underlying file system, it the ancest
  // should not be ended with '/'.
  if ((ancest.size() + 1) >= descent.size()) {
    return false;
  }
  assert(ancest.back() != '/');
  return descent.find(ancest) == 0 && descent[ancest.size()] == '/';
}

static const std::string
relative_path(const std::string& ancest, const std::string& descent) {
  assert(ancest_of_path(ancest, descent));
  auto pos = ancest.size() + 1;
  return descent.substr(pos);
}

ogl_dir*
ogl_repo::get_parent_dir(const std::string &path,
                         std::string &basename) {
  auto sep = path.rfind('/');
  if (sep == std::string::npos) {
    return nullptr;
  }
  auto dirname = sep == 0 ? std::string("/") : path.substr(0, sep);
  basename = path.substr(sep + 1);
  if (basename.size() == 0) {
    return nullptr;
  }
  auto dir = find_dir(dirname);
  return dir;
}

bool
ogl_repo::add_file(const std::string &path) {
  std::string basename;
  auto dir = get_parent_dir(path, basename);
  if (dir == nullptr) {
    return false;
  }
  return dir->add_file(basename);
}

bool
ogl_repo::add_dir(const std::string &path) {
  std::string basename;
  auto dir = get_parent_dir(path, basename);
  if (dir == nullptr) {
    return false;
  }
  return dir->add_dir(basename);
}

bool
ogl_repo::add_symlink(const std::string &path) {
  std::string basename;
  auto dir = get_parent_dir(path, basename);
  if (dir == nullptr) {
    return false;
  }
  return dir->add_symlink(basename);
}

bool
ogl_repo::remove(const std::string &path) {
  std::string basename;
  auto dir = get_parent_dir(path, basename);
  if (dir == nullptr) {
    return false;
  }
  return dir->remove(basename);
}

bool
ogl_repo::mark_local(const std::string &path) {
  std::string basename;
  auto dir = get_parent_dir(path, basename);
  if (dir == nullptr) {
    return false;
  }
  return dir->mark_local(basename);
}

bool
ogl_repo::mark_nonexistent(const std::string &path) {
  std::string basename;
  auto dir = get_parent_dir(path, basename);
  if (dir == nullptr) {
    return false;
  }
  return dir->mark_nonexistent(basename);
}

ogl_entry*
ogl_repo::find(const std::string& path) {
  if (path == root_path) {
    return root_dir.get();
  }

  // Must be one of descendants of the root path.
  assert(ancest_of_path(root_path, path));

  // Lookup objects along the path from the root.
  ogl_entry* ent = root_dir.get();
  std::string rpath = relative_path(root_path, path);
  while (ent && rpath.size()) {
    ogl_dir* dir = ent->to_dir();
    if (dir == nullptr) {
      // The visiting entry is not a directory, it can be a file,
      // a link, or not existing.
      return nullptr;
    }
    if (!dir->has_loaded()) {
      dir->load();
    }

    auto pos = rpath.find('/');
    std::string name;
    assert(pos != 0);
    if (pos != std::string::npos) {
      name = rpath.substr(0, pos);
      rpath = rpath.substr(pos + 1);
    } else {
      name = rpath;
      rpath = "";
    }
    ent = dir->lookup(name);
  }
  return ent;
}

/**
 * Update the root-ref in the repository.
 */
bool
ogl_repo::update_root_ref() {
  auto ok = write_root_ref(repo_path, root_dir->hashcode());
  return ok;
}

bool
ogl_repo::store_obj(uint64_t hash, const otypes::object* obj) {
  assert(obj->magic == otypes::object::MAGIC);
  char hashstr[24];
  snprintf(hashstr, sizeof(hashstr), "%016lx", hash);
  std::string _path = repo_path + "/objects/" + hashstr;
  const char *path = _path.c_str();
  struct stat statbuf;
  if (stat(path, &statbuf) == 0) {
    // An existing object!
    return true;
  }
  int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  auto cp = write(fd, obj, obj->size);
  return cp == obj->size;
}

std::unique_ptr<otypes::object>
ogl_repo::load_obj(uint64_t hash) {
  char hashstr[24];
  snprintf(hashstr, sizeof(hashstr), "%016lx", hash);
  std::string _path = repo_path + "/objects/" + hashstr;
  const char* path = _path.c_str();
  int fd = open(path, O_RDONLY);
  otypes::object obj;
  auto cp = read(fd, &obj, sizeof(obj));
  if (cp != sizeof(obj)) {
    return nullptr;
  }
  assert(obj.magic == otypes::object::MAGIC);
  assert(obj.size >= sizeof(obj));

  char *buf = new char[obj.size];
  assert(buf);

  memcpy(buf, &obj, sizeof(obj));
  cp = read(fd, buf + sizeof(obj), obj.size - sizeof(obj));
  assert(cp == obj.size - sizeof(obj));

  std::unique_ptr<otypes::object> robj(reinterpret_cast<otypes::object*>(buf));

  return robj;
}

void
ogl_dir::diff(const ogl_dir* other, const diff_handler& handler) const {
  std::vector<const std::string*> common;
  // Find new names
  for (auto v = entries.begin(); v != entries.end(); ++v) {
    if (other->entries.find(v->first) != other->entries.end()) {
      common.push_back(&v->first);
    } else {
      auto ok = handler(DIFF_ADD, this, other, v->first);
      if (!ok) {
        return;
      }
    }
  }
  // Find removed names
  for (auto v = other->entries.begin(); v != other->entries.end(); ++v) {
    if (entries.find(v->first) == entries.end()) {
      auto ok = handler(DIFF_RM, this, other, v->first);
      if (!ok) {
        return;
      }
    }
  }
  // Find modified names
  for (auto name_v = common.begin(); name_v != common.end(); ++name_v) {
    auto ent = lookup(**name_v);
    auto oent = other->lookup(**name_v);
    if (ent->get_type() != oent->get_type()) {
      auto ok = handler(DIFF_MOD, this, other, **name_v);
      if (!ok) {
        return;
      }
    } else {
      switch (ent->get_type()) {
      case OGL_REMOVED:
      case OGL_NONEXISTENT:
        break;

      case OGL_FILE:
        if (ent->to_file()->hashcode() !=
            oent->to_file()->hashcode()) {
          auto ok = handler(DIFF_MOD, this, other, **name_v);
          if (!ok) {
            return;
          }
        }
        break;

      case OGL_DIR:
        if (ent->to_dir()->hashcode() !=
            oent->to_dir()->hashcode()) {
          auto ok = handler(DIFF_MOD, this, other, **name_v);
          if (!ok) {
            return;
          }
        }
        break;

      case OGL_SYMLINK:
        if (ent->to_symlink()->hashcode() !=
            oent->to_symlink()->hashcode()) {
          auto ok = handler(DIFF_MOD, this, other, **name_v);
          if (!ok) {
            return;
          }
        }
        break;

      case OGL_LOCAL:
        break;

      default:
        ABORT("invalid ogl_type");
        break;
      }
    }
  }
}

void
ogl_dir::copy_to(ogl_dir* dst) const {
  assert(get_type() == dst->get_type());
  dst->hash = hash;
  dst->mode = mode;
  dst->own = own;
  dst->own_group = own_group;
  if (modified) {
    for (auto ent = entries.begin();
         ent != entries.end();
         ++ent) {
      dst->entries[ent->first] = ent->second->clone();
    }
  } // else make dst unloaded. And, |hash| should have a valid value.
  dst->modified = modified;
}

bool
ogl_repo::merge(ogl_repo* src, ogl_repo* dst, ogl_repo* common) {
  auto sroot = src->get_root();
  auto droot = dst->get_root();
  auto croot = common->get_root();
  std::vector<std::string> dir_queue;

  bool conflict = false;
  auto check_conflictions = [&](ogl_dir::diff_ops op,
                                const ogl_dir* srcparent,
                                const ogl_dir* cmmparent,
                                const std::string& name) -> bool {
    auto dstparent = dst->find_dir(srcparent->get_path());
    switch (op) {
    case ogl_dir::DIFF_ADD:
      {
        if (dstparent->lookup(name)) {
          // Add a name that is already in dst.
          conflict = true;
          return false;
        }
      }
      break;

    case ogl_dir::DIFF_RM:
      {
        if (dstparent == nullptr) {
          // Remove a name in a directory that is not existing in dst.
          conflict = true;
          return false;
        }
        auto dstent = dstparent->lookup(name);
        if (dstent == nullptr) {
          // The name is not existing in dst.
          conflict = true;
          return false;
        }
        // Check if the entry is the same in common and dst.
        auto cmment = cmmparent->lookup(name);
        assert(cmment);
        if (cmment->get_type() != dstent->get_type()) {
          // The type of the name has been modified in dst.
          conflict = true;
          return false;
        }
        switch (dstent->get_type()) {
        case ogl_entry::OGL_NONEXISTENT:
          break;

        case ogl_entry::OGL_FILE:
          {
            if (dstent->to_file()->hashcode() !=
                cmment->to_file()->hashcode()) {
              conflict = true;
              return false;
            }
          }
          break;

        case ogl_entry::OGL_DIR:
          {
            if (dstent->to_dir()->hashcode() !=
                cmment->to_dir()->hashcode()) {
              conflict = true;
              return false;
            }
          }
          break;

        case ogl_entry::OGL_SYMLINK:
          {
            if (dstent->to_symlink()->hashcode() !=
                cmment->to_symlink()->hashcode()) {
              conflict = true;
              return false;
            }
          }
          break;

        case ogl_entry::OGL_LOCAL:
          break;

        default:
          ABORT("invalid ogl_type");
        }
      }
      break;

    case ogl_dir::DIFF_MOD:
      {
        if (dstparent == nullptr) {
          // Modify a name in a directory that is not existing in dst.
          conflict = true;
          return false;
        }
        auto dstent = dstparent->lookup(name);
        if (dstent == nullptr) {
          // The name is not existing in dst.
          conflict = true;
          return false;
        }

        // Check if the entry is the same in common and dst.
        auto cmment = cmmparent->lookup(name);
        assert(cmment);
        if (cmment->get_type() != dstent->get_type()) {
          // The type of the name has been modified in dst.
          conflict = true;
          return false;
        }
        switch (dstent->get_type()) {
        case ogl_entry::OGL_NONEXISTENT:
          break;

        case ogl_entry::OGL_FILE:
          {
            if (dstent->to_file()->hashcode() !=
                cmment->to_file()->hashcode()) {
              conflict = true;
              return false;
            }
          }
          break;

        case ogl_entry::OGL_DIR:
          {
            // Directories are exceptions, that they can be modified
            // in dst as long as keeping as a directory.  In this
            // case, conflictions happen only if any of their entries
            // have conflictions.
            auto ent = srcparent->lookup(name);
            if (ent->get_type() == ogl_entry::OGL_DIR) {
              // Both entries are ogl_dir.
              // Descend to the directory to change entries.
              dir_queue.push_back(srcparent->get_path(name));
            }
          }
          break;

        case ogl_entry::OGL_SYMLINK:
          {
            if (dstent->to_symlink()->hashcode() !=
                cmment->to_symlink()->hashcode()) {
              conflict = true;
              return false;
            }
          }
          break;

        case ogl_entry::OGL_LOCAL:
          break;

        default:
          ABORT("invalid ogl_type");
        }
      }
      break;

    default:
      ABORT("invalid value of diff_ops");
    }

    return true;
  };
  auto apply_changes = [&](ogl_dir::diff_ops op,
                           const ogl_dir* srcparent,
                           const ogl_dir* cmmparent,
                           const std::string& name) -> bool {
    auto dstparent = dst->find_dir(srcparent->get_path());
    switch (op) {
    case ogl_dir::DIFF_MOD:
      {
        if (srcparent->lookup(name)->get_type() == ogl_entry::OGL_DIR &&
            dstparent->lookup(name)->get_type() == ogl_entry::OGL_DIR) {
          // The entry of the name is a directory in both src and dst.
          // Descend down the tree to modify directories.
          dir_queue.push_back(dstparent->get_path(name));
          break;
        }
        // Repalce the name with a new ogl_dir.
        dstparent->remove(name);
      }
      break;

    case ogl_dir::DIFF_ADD:
      {
        auto ent = srcparent->lookup(name);
        assert(ent);
        switch (ent->get_type()) {
        case ogl_entry::OGL_NONEXISTENT:
          {
            auto ok = dstparent->mark_nonexistent(name);
            assert(ok);
          }
          break;

        case ogl_entry::OGL_FILE:
          {
            auto ok = dstparent->add_file(name);
            assert(ok);
            auto file = dstparent->lookup(name)->to_file();
            file->set_hashcode(ent->to_file()->hashcode());
          }
          break;

        case ogl_entry::OGL_DIR:
          {
            auto ok = dstparent->add_dir(name);
            assert(ok);
            auto dstdir = dstparent->lookup(name)->to_dir();
            auto dir = ent->to_dir();
            dir->copy_to(dstdir);
          }
          break;

        case ogl_entry::OGL_SYMLINK:
          {
            auto ok = dstparent->add_symlink(name);
            assert(ok);
            auto symlink = dstparent->lookup(name)->to_symlink();
            symlink->set_hashcode(ent->to_symlink()->hashcode());
          }
          break;

        case ogl_entry::OGL_LOCAL:
          {
            auto ok = dstparent->mark_local(name);
            assert(ok);
          }
          break;

        default:
          ABORT("invalid ogl_type");
        }
      }
      break;

    case ogl_dir::DIFF_RM:
      {
        dstparent->remove(name);
      }
      break;

    default:
      ABORT("invalid value of diff_ops");
    }

    return true;
  };

  // Make sure not any conflictions
  dir_queue.push_back(src->get_rootpath());
  while (dir_queue.size()) {
    auto dirname = dir_queue.back();
    dir_queue.pop_back();

    auto ent = src->find(dirname);
    auto dir = ent->to_dir();
    assert(dir);
    auto cmment = common->find(dirname);
    auto cmmdir = cmment->to_dir();
    assert(cmmdir);
    dir->diff(cmmdir, check_conflictions);
    if (conflict) {
      return false;
    }
  }

  // Apply changes
  dir_queue.push_back(src->get_rootpath());
  while (dir_queue.size()) {
    auto dirname = dir_queue.back();
    dir_queue.pop_back();

    auto ent = src->find(dirname);
    auto dir = ent->to_dir();
    assert(dir);
    auto cmment = common->find(dirname);
    auto cmmdir = cmment->to_dir();
    assert(cmmdir);
    dir->diff(cmmdir, apply_changes);
  }
  return true;
}
