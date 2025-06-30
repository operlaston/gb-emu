#include "joypad.hh"
#include "SDL2/SDL_events.h"
#include "SDL2/SDL_keycode.h"
#include "constants.hh"

Joypad::Joypad(Memory &m) : mmu(m) {
  key_state = 0xFF;
  joypad = 0xFF;
  quit = false;
}

void Joypad::key_pressed(uint8_t key) { 
  if (((key_state >> key) & 1) == 0) {
    return;
  }
  key_state &= ~(1 << key);
}
void Joypad::key_released(uint8_t key) {
  key_state |= (1 << key);
}

void Joypad::set_joypad_state(uint8_t joypad_state) {
  joypad = (joypad & 0x0F) | (joypad_state & 0xF0);
}

uint8_t Joypad::get_joypad_state() {
  // uint8_t joypad = mmu.get_joypad_reg();
  uint8_t prev_keys = joypad & 0x0F;
  if (((joypad >> 4) & 1) == 0) { // directional buttons enabled
    joypad = (joypad & 0xF0) | (key_state >> 4);
  }
  else if (((joypad >> 5) & 1) == 0) { // standard buttons enabled
    joypad = (joypad & 0xF0) | (key_state & 0x0F);
  }
  // printf("prev keys: %d\n", prev_keys);
  // printf("after keys: %d\n", joypad & 0x0F);
  if (prev_keys == 0b1111 && (joypad & 0x0F) != 0b1111) {
    mmu.request_interrupt(JOYPAD_INTER);
    // printf("interrupt requested\n");
  }
  return joypad;
}

void Joypad::handle_input() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      quit = true;
      return;
    }

    else if (event.type == SDL_KEYDOWN) {
      switch(event.key.keysym.sym) {
        case SDLK_ESCAPE:
          quit = true;
          break;
        case SDLK_UP:
          key_pressed(KEY_UP);
          break;
        case SDLK_DOWN:
          key_pressed(KEY_DOWN);
          break;
        case SDLK_LEFT:
          key_pressed(KEY_LEFT);
          break;
        case SDLK_RIGHT:
          key_pressed(KEY_RIGHT);
          break;
        case SDLK_z:
          key_pressed(KEY_B);
          break;
        case SDLK_x:
          key_pressed(KEY_A);
          break;
        case SDLK_TAB:
          key_pressed(KEY_SELECT);
          break;
        case SDLK_RETURN:
          key_pressed(KEY_START);
          break; 
        default:
          break;
      }
    }
    else if (event.type == SDL_KEYUP) {
      switch(event.key.keysym.sym) {
        case SDLK_UP:
          key_released(KEY_UP);
          break;
        case SDLK_DOWN:
          key_released(KEY_DOWN);
          break;
        case SDLK_LEFT:
          key_released(KEY_LEFT);
          break;
        case SDLK_RIGHT:
          key_released(KEY_RIGHT);
          break;
        case SDLK_z:
          key_released(KEY_B);
          break;
        case SDLK_x:
          key_released(KEY_A);
          break;
        case SDLK_TAB:
          key_released(KEY_SELECT);
          break;
        case SDLK_RETURN:
          key_released(KEY_START);
          break; 
        default:
          break;
      }
    }
  }
}
