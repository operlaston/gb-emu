#ifndef GPU_H
#define GPU_H

#include "memory.hh"
#include <cstdint>
#include <SDL2/SDL.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_error.h>

// bits for the LCD control register
typedef enum {
  BG_WIN_ENABLE = 0,
  OBJ_ENABLE = 1,
  OBJ_SIZE = 2,
  BG_TILE_MAP = 3,
  TILE_DATA = 4,
  WIN_ENABLE = 5,
  WIN_TILE_MAP = 6,
  LCD_ENABLE = 7
} LCD_CONTROL_BIT;

typedef enum {
  LYC_EQUALS_LY = 2,
  MODE_0 = 3,
  MODE_1 = 4,
  MODE_2 = 5,
  LYC_INT = 6
} LCD_STAT_BIT;

#define TILE_DATA_LENGTH (0x1000)
#define MODE_2_CYCLES (80) // t-cycles
#define MODE_3_CYCLES (172)
#define MODE_0_CYCLES (204)
#define MODE_1_CYCLES (456)
#define TILE_MAP_LENGTH (0x400)
#define SCREEN_WIDTH (160)
#define SCREEN_HEIGHT (144)
#define SCALE_FACTOR (3)

class Gpu {
  Memory& mmu;
  uint16_t mode_clock;
  bool win_enable;
  bool sprite_enable;
  bool lcd_enable;
  bool bg_win_enable;
  uint8_t mode;
  uint16_t win_tile_map_base;
  uint16_t bg_tile_map_base;
  uint16_t tile_data_base;
  uint8_t sprite_height;
  uint8_t screen[SCREEN_HEIGHT][SCREEN_WIDTH];
  uint8_t num_sprites;
  uint8_t scx;
  uint8_t scy;
  uint8_t wy;
  uint8_t wx;
  uint8_t curr_x; // relative to scx so curr_x = 0 means scx + x

  bool get_lcdc_bit(LCD_CONTROL_BIT);
  bool get_stat_bit(LCD_STAT_BIT);
  void draw_tile_line(uint8_t);
  void draw_sprite_tile_line(int16_t, int16_t, int16_t, uint8_t, uint8_t);
  void set_draw_color(uint8_t);
  void set_lcdc_status();
  void draw_line();
  void set_mode(uint8_t);

  // SDL
  SDL_Window *window;
  SDL_Renderer *renderer;

public:
  Gpu(Memory& mem);
  // use default destructor
  void step(uint8_t);
  void render();
};

#endif
