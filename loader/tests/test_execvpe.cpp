#include <unistd.h>
#include <stdio.h>
#include <string.h>

int
main() {
  printf("execvpe\n");
  char* const argv[] = { strdup("ls"), nullptr };
  auto r = execvpe("ls", argv, environ);
  if (r < 0) {
    perror("execvpe");
  } else {
    printf("OK\n");
  }
  return 0;
}
