#include <stdio.h>
#include <unistd.h>

int main() {
  char msg[] = "Hello! This is the fake trampoline.\n";
  write(1, msg, sizeof(msg) - 1);
  return 0;
}
