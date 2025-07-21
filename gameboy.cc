#include "gameboy.hh"
#include "constants.hh"
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_timer.h>
#include <iostream>

Gameboy::Gameboy(char *rom_file)
    : mmu(rom_file), cpu(mmu), gpu(mmu), timer(mmu), joypad(mmu) {
  mmu.set_timer(&timer);
  mmu.set_joypad(&joypad);
  mmu.set_cpu(&cpu);
  init_sdl();
  gpu.init_sdl(renderer, texture);
}

void Gameboy::init_sdl() {
  // init SDL
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
    std::cerr << "Could not initialize SDL: " << SDL_GetError() << std::endl;
    std::exit(1);
  }

  // init SDL window
  window = SDL_CreateWindow("gb-emu", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH * SCALE_FACTOR,
                            SCREEN_HEIGHT * SCALE_FACTOR,
                            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (window == NULL) {
    std::cerr << "Could not create SDL window: " << SDL_GetError() << std::endl;
    std::exit(1);
  }

  // init SDL renderer
  renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED /*| SDL_RENDERER_PRESENTVSYNC*/);
  if (renderer == NULL) {
    std::cerr << "Could not create SDL renderer: " << SDL_GetError()
              << std::endl;
    std::exit(1);
  }
  SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);

  // init SDL texture
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                              SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH,
                              SCREEN_HEIGHT);
  if (texture == NULL) {
    std::cerr << "Could not create SDL texture: " << SDL_GetError()
              << std::endl;
    std::exit(1);
  }
}

void Gameboy::shutdown_sdl() {
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void Gameboy::start() {
  while (!joypad.quit) {
    joypad.handle_input();
    update();
  }
  mmu.save_ram();
  shutdown_sdl();
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
    if (cpu.state == RUNNING || cpu.state == BOOTING) {
      cycles = cpu.fetch_and_execute();
    }
    else if (cpu.state == HALTED)
      cycles = 4; // 1 m-cycle
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
    } else {
      interrupt_cycles = 0;
    }
  }
  const uint64_t end_time = SDL_GetPerformanceCounter();
  const double time_spent =
      (double)((end_time - start_time) * 1000) /
      SDL_GetPerformanceFrequency(); // time spent in milliseconds

  // 1x (normal) speed
  // double delay = 16.7427 - time_spent;

  // 2x (double) speed
  // double delay = 8.37135 - time_spent;

  // 4x (quadruple) speed
  // double delay = 4.185675 - time_spent;

  double delay = joypad.speed - time_spent;

  if (delay > 1.0) {
    SDL_Delay((Uint32) delay);
  }
}
