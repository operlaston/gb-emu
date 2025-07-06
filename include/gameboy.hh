#ifndef GAMEBOY_H
#define GAMEBOY_H

#include "cpu.hh"
#include "gpu.hh"
#include "joypad.hh"
#include "memory.hh"
#include "timer.hh"
#include <SDL2/SDL.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>

class Gameboy {

  Memory mmu;
  Cpu cpu;
  Gpu gpu;
  Timer timer;
  Joypad joypad;

  // SDL
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  void init_sdl();
  void shutdown_sdl();

public:
  Gameboy(char *rom_file);
  // default destructor
  void start();
  void update();
};

#endif
