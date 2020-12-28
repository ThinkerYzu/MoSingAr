/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
#include "ogl.h"

int
main(int argc, char * const * argv) {
  std::string root("..");
  std::string repo_path("test_repo");
  auto ok = ogl_repo::init(repo_path);
  if (!ok) {
    printf("FAILED!\n");
    return 255;
  }
  ogl_repo repo(root, repo_path);
  printf("OK!\n");
  return 0;
}
