#include "gameboy.hh"
#include "constants.hh"

Gameboy::Gameboy(char *rom_path)
  : mmu(rom_path),
    cpu(mmu) {
}

void Gameboy::update() {
  // max cycles per frame (60 frames per second)
  const int CYCLES_PER_FRAME = CYCLES_PER_SECOND / 60;
  int cycles_this_update = 0;
  while (cycles_this_update < CYCLES_PER_FRAME) {
    // perform a cycle
    uint8_t cycles = 0;
    if (cpu.state == RUNNING) cycles = cpu.fetch_and_execute();
    else if (cpu.state == HALTED) cycles = 1;
    cycles_this_update += cycles;
    // update timers
    cpu.update_timers(cycles);
    // update graphics
    // do interrupts
    cpu.service_interrupt();
  }
  // render the screen
}
