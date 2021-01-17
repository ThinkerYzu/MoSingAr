/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "ogl.h"

#include <limits.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

int
main(int argc, char * const * argv) {
  char* path = realpath("..", nullptr);
  std::string root(path);
  free(path);
  std::string repo_path("test_repo");

  auto ok = ogl_repo::init(repo_path);
  if (!ok) {
    printf("FAILED!\n");
    return 255;
  }
  ogl_repo repo(root, repo_path);
  printf("OK!\n");

  std::string tests("tests");
  auto rootobj = repo.get_root();
  rootobj->add_dir(tests);

  auto tests_ent = repo.find(realpath("../tests", nullptr));
  assert(tests_ent);
  auto tests_dir = tests_ent->to_dir();
  assert(tests_dir);
  tests_dir->add_file("basic_objects");

  auto r = symlink("basic_objects", "symlink-test");
  assert(r == 0);
  tests_dir->add_symlink("symlink-test");

  repo.commit();

  // Check if the repo can be loaded correctly.
  ogl_repo repo1(root, repo_path);
  auto bo_ent = repo.find(realpath("../tests/basic_objects", nullptr));
  assert(bo_ent);
  auto bo_file = bo_ent->to_file();
  assert(bo_file);
  auto hash = bo_file->hashcode();
  ok = bo_file->compute_hashcode();
  assert(ok);
  assert(hash == bo_file->hashcode());

  auto sl_ent = repo1.find((std::string(realpath("../tests", nullptr)) + "/symlink-test").c_str());
  assert(sl_ent);
  auto sl_link = sl_ent->to_symlink();
  assert(sl_link);
  ok = sl_link->load();
  assert(ok);
  hash = sl_link->hashcode();
  assert(hash);
  auto target = sl_link->get_target();
  assert(target == "basic_objects");

  // Adding the same file more than once.
  tests_ent = repo1.find(realpath("../tests", nullptr));
  assert(tests_ent);
  tests_dir = tests_ent->to_dir();
  assert(tests_dir);
  ok = tests_dir->add_file("basic_objects");
  assert(!ok);

  // Remove
  ok = tests_dir->remove("basic_objects");
  assert(ok);
  bo_ent = repo1.find(realpath("../tests/basic_objects", nullptr));
  assert(bo_ent == nullptr);
  repo1.commit();
  bo_ent = repo1.find(realpath("../tests/basic_objects", nullptr));
  assert(bo_ent == nullptr);

  // Check if basic_objects has been removed from the repo.
  ogl_repo repo2(root, repo_path);
  bo_ent = repo2.find(realpath("../tests/basic_objects", nullptr));
  assert(bo_ent == nullptr);

  // Mark a file a nonexistent.
  tests_ent = repo2.find(realpath("../tests", nullptr));
  assert(tests_ent);
  tests_dir = tests_ent->to_dir();
  assert(tests_dir);
  ok = tests_dir->mark_nonexistent("basic_objects");
  assert(ok);
  repo2.commit();

  // Read the nonexistent object back.
  ogl_repo repo3(root, repo_path);
  bo_ent = repo3.find(realpath("../tests/basic_objects", nullptr));
  assert(bo_ent != nullptr);
  assert(bo_ent->to_nonexistent());

  // Test repo's operators.
  ok = repo3.remove(realpath("../tests/basic_objects", nullptr));
  assert(ok);
  ok = repo3.add_file(realpath("../tests/basic_objects", nullptr));
  assert(ok);
  bo_ent = repo3.find(realpath("../tests/basic_objects", nullptr));
  assert(bo_ent != nullptr);
  bo_file = bo_ent->to_file();
  assert(bo_file != nullptr);

  return 0;
}
