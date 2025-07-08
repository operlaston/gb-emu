#ifndef JOYPAD_H
#define JOYPAD_H

#include "stdint.h"
#include "memory.hh"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>

#define JOYPAD_REG (0xFF00)

typedef enum {
  KEY_A = 0,
  KEY_B = 1,
  KEY_SELECT = 2,
  KEY_START = 3,
  KEY_RIGHT = 4,
  KEY_LEFT = 5,
  KEY_UP = 6,
  KEY_DOWN = 7
} keys;


class Joypad {
  Memory &mmu;

  uint8_t key_state; // standard buttons in lower nibble, directional in upper nibble
  uint8_t joypad;
  void key_pressed(uint8_t key);
  void key_released(uint8_t key);
public:
  bool quit;
  double speed;
  Joypad(Memory &m);
  void set_joypad_state(uint8_t joypad_state);
  uint8_t get_joypad_state();
  void handle_input();
};

#endif
