#include <stdio.h>
#include "gameboy.hh"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: gameboy [path/to/rom]");
    exit(1);
  }

  Gameboy gameboy(argv[1]);
  gameboy.start();
  return 1;
}
