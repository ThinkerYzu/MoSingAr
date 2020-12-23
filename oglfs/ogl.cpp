/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
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

static uint64_t
compute_hash_buf(void* buf, int size) {
  return 0;
}

bool
ogl_dir::add_file(const std::string& filename) {
  entries[filename] = std::make_unique<ogl_file>(repo, filename, abspath);
  modified = true;
  return true;
}

bool
ogl_dir::add_dir(const std::string& dirname) {
  std::unique_ptr<ogl_dir> dir = std::make_unique<ogl_dir>(repo, this, dirname);
  dir->modified = true;
  entries[dirname] = std::move(dir);
  modified = true;
  return true;
}

bool
ogl_dir::dump() {
  assert(loaded);

  std::vector<std::string> names(entries.size());
  int cnt_nonexistent = 0;
  int cnt_file = 0;
  int cnt_dir = 0;
  int cnt_symlink = 0;
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
    }
  }
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

  std::sort(names.begin(), names.end());
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
        entobj->tm  = 0;
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
        entobj->tm = static_cast<uint32_t>(time(nullptr));
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
        entobj->tm = static_cast<uint32_t>(time(nullptr));
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
        entobj->tm = static_cast<uint32_t>(time(nullptr));
        hashcodes[ndx] = symlink->hashcode();
      }
      break;
    }
  }

  auto ok = repo->store_obj(hashcode(), obj);

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
          std::make_unique<ogl_file>(repo, name, abspath);
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
          std::make_unique<ogl_symlink>(repo);
        symlink->set_hashcode(hash);
        entries[name] = std::move(symlink);
      }
      break;

    default:
      ABORT("Unknown object type");
    }
  }
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
  uint64_t hash = atoll(buf);
  root_dir = std::make_unique<ogl_dir>(this, nullptr, root);
  root_dir->set_hashcode(hash);
  bool ok = root_dir->load();
  assert(ok);
}

bool
ogl_repo::store_obj(uint64_t hash, const otypes::object* obj) {
  assert(obj->magic == otypes::object::MAGIC);
  std::string _path = repo_path + "/" + std::to_string(hash);
  const char *path = _path.c_str();
  int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY);
  auto cp = write(fd, obj, obj->size);
  return cp == obj->size;
}

std::unique_ptr<otypes::object>
ogl_repo::load_obj(uint64_t hash) {
  std::string _path = repo_path + "/" + std::to_string(hash);
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
