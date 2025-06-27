#include "gameboy.hh"
#include "constants.hh"
// #include <SDL2/SDL.h>
// #include <SDL2/SDL_error.h>
// #include <SDL2/SDL_render.h>
// #include <SDL2/SDL_timer.h>
// #include <SDL2/SDL_video.h>

Gameboy::Gameboy(char *rom_path)
  : mmu(rom_path),
    cpu(mmu),
    gpu(mmu),
    timer(mmu){
  mmu.set_timer(&timer);
  printf("completed initalization\n");
}

void Gameboy::update() {
  // max cycles per frame (59.7275 frames per second)
  const int CYCLES_PER_FRAME = CYCLES_PER_SECOND / 59.7275;
  int cycles_this_update = 0;
  uint8_t interrupt_cycles = 0;
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
  // render the screen
  gpu.render();
}
