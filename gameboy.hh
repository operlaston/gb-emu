#ifndef GAMEBOY_H
#define GAMEBOY_H

#include "cpu.hh"
#include "memory.hh"
#include "gpu.hh"
#include "timer.hh"

class Gameboy {

  Memory mmu;
  Cpu cpu;
  Gpu gpu;
  Timer timer;

public:
  Gameboy(char *rom_path);
  // default destructor
  void update();
};

#endif
