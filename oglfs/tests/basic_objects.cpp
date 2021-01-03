/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "ogl.h"

#include <limits.h>
#include <stdlib.h>
#include <assert.h>

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

  repo.commit();
  return 0;
}
