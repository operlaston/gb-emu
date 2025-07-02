#ifndef GAMEBOY_H
#define GAMEBOY_H

#include "cpu.hh"
#include "memory.hh"
#include "gpu.hh"
#include "timer.hh"
#include "joypad.hh"

class Gameboy {

  Memory mmu;
  Cpu cpu;
  Gpu gpu;
  Timer timer;
  Joypad joypad;

public:
  Gameboy(char *rom_file);
  // default destructor
  void start();
  void update();
};

#endif
