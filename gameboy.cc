#include "gameboy.hh"
#include "SDL2/SDL_stdinc.h"
#include "constants.hh"
#include <SDL2/SDL_timer.h>

Gameboy::Gameboy(char *rom_file)
  : mmu(rom_file),
    cpu(mmu),
    gpu(mmu),
    timer(mmu),
    joypad(mmu){
  mmu.set_timer(&timer);
  mmu.set_joypad(&joypad);
}

void Gameboy::start() {
  while (!joypad.quit) {
    joypad.handle_input();
    update();
  }
}

void Gameboy::update() {
  // max cycles per frame (59.7275 frames per second)
  const int CYCLES_PER_FRAME = CYCLES_PER_SECOND / 59.7275;
  // const int CYCLES_PER_FRAME = CYCLES_PER_SECOND / 59.7;
  // const int CYCLES_PER_FRAME = 70224;

  int cycles_this_update = 0;
  uint8_t interrupt_cycles = 0;

  const uint64_t start_time = SDL_GetPerformanceCounter(); 

  while (cycles_this_update < CYCLES_PER_FRAME) {
    // perform a cycle
    // uint8_t cycles = interrupt_cycles;
    uint8_t cycles = interrupt_cycles;
    if (cpu.state == RUNNING) cycles = cpu.fetch_and_execute();
    else if (cpu.state == HALTED) cycles = 4; // 1 m-cycle
    cycles_this_update += cycles;
    for (int i = 0; i < cycles; i++) {
      // update_timers
      timer.tick();
    }
    // update graphics
    gpu.step(cycles);
    // do interrupts
    if (cpu.service_interrupt()) {
      // an interrupt takes 5 m-cycles
      interrupt_cycles = 20;
    }
    else {
      interrupt_cycles = 0;
    }
  }
  const uint64_t end_time = SDL_GetPerformanceCounter();
  const double time_spent = (double)((end_time - start_time) * 1000) / 
                              SDL_GetPerformanceFrequency(); // time spent in milliseconds
  double delay = 16.7427 - time_spent;
  if (delay > 1.0) {
    SDL_Delay((Uint32) delay);
  }
}
