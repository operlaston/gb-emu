#include <stdio.h>
#include "cpu.hh"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: gameboy [path/to/rom]");
    exit(1);
  }
  Cpu emu(argv[1]);
  while(1){
    emu.update();
  }
  return 1;
}
