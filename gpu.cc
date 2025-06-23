#include "gpu.hh"
#include "constants.hh"
#include <cstdlib>
#include <cstring>
#include <stdio.h>

Gpu::Gpu(Memory& mem) : mmu(mem) {
  mode_clock = 0;
  memset(screen, 0, sizeof(screen));
  memset(sprite_buf, 0, sizeof(sprite_buf));
  set_mode(2);
  win_enable = 0;
  sprite_enable = 0;
  lcd_enable = 0;
  bg_win_enable = 0;
  win_tile_map_addr = 0;
  bg_tile_map_addr = 0;
  tile_data_addr = 0;
  sprite_height = 0;
  num_sprites = 0;
  scx = 0;
  scy = 0;
  wy = 0;
  wx = 0;

  // TODO: initialize an sdl window
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) < 0) {
    SDL_Log("Could not initialize SDL: %s\n", SDL_GetError());
    exit(1);
  }

  window = SDL_CreateWindow("Operlaston's Gameboy Emulator", 0, 0,
                            SCREEN_WIDTH * SCALE_FACTOR,
                            SCREEN_HEIGHT * SCALE_FACTOR, 0);
  if (window == NULL) {
    SDL_Log("Could not create SDL window: %s\n", SDL_GetError());
    exit(1);
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (renderer == NULL) {
    SDL_Log("Could not create SDL renderer: %s\n", SDL_GetError());
    exit(1);
  }

  printf("initialized gpu\n");
}

bool Gpu::get_lcdc_bit(LCD_CONTROL_BIT bit) {
  uint8_t lcdc_reg = mmu.read_byte(LCD_CONTROL);
  return lcdc_reg & (1 << bit);
}

void Gpu::set_lcdc_status() {
  lcd_enable = get_lcdc_bit(LCD_ENABLE) ? 1 : 0;
  win_enable = get_lcdc_bit(WIN_ENABLE) ? 1 : 0;
  sprite_enable = get_lcdc_bit(OBJ_ENABLE) ? 1 : 0;
  win_tile_map_addr = get_lcdc_bit(WIN_TILE_MAP) ? 0x9C00 : 0x9800;
  bg_tile_map_addr = get_lcdc_bit(BG_TILE_MAP) ? 0x9C00 : 0x9800;
  tile_data_addr = get_lcdc_bit(TILE_DATA) ? 0x8000 : 0x9000;
  sprite_height = get_lcdc_bit(OBJ_SIZE) ? 8 : 16;
  sprite_enable = get_lcdc_bit(OBJ_ENABLE) ? 1 : 0;
  bg_win_enable = get_lcdc_bit(BG_WIN_ENABLE) ? 1 : 0;
}

void Gpu::set_mode(uint8_t mode) {
  this->mode = mode;
  mmu.write_byte(0xFF41, mmu.read_byte(0xFF41) | mode);
}

uint8_t Gpu::draw_bg_pixel(uint8_t x, uint8_t curr_line) {
  // retrieve the tile index
  uint8_t pixel_x = (x + scx) & 0xFF;
  uint8_t pixel_y = (curr_line + scy) & 0xFF;
  uint8_t tile_x = pixel_x >> 3; // divided by 8
  uint8_t tile_y = pixel_y >> 3;
  uint16_t tile_index_addr = bg_tile_map_addr + (tile_y << 5) + tile_x; // times 32
  uint8_t tile_index = mmu.read_byte(tile_index_addr);

  // find the location of tile data using the tile index
  uint16_t tile_addr = tile_data_addr + (tile_index << 4); // each tile is 16 bytes // << 4 is * 16
  if (tile_data_addr == 0x9000) tile_addr = tile_data_addr + ((int8_t)tile_index << 4);

  // retrieve tile data
  uint8_t tile_px = pixel_x & 0x7;
  uint8_t tile_py = pixel_y & 0x7;
  uint8_t byte1 = mmu.read_byte(tile_addr + (tile_py >> 1));
  uint8_t byte2 = mmu.read_byte(tile_addr + (tile_py >> 1) + 1);

  // & bytes together to get pixels
  uint8_t bit = 7 - tile_px;
  uint8_t color_id = (((byte2 >> bit) & 1) << 1) | ((byte1 >> bit) & 1);
  uint8_t palette = mmu.read_byte(0xFF47);
  uint8_t color = (palette >> (color_id << 1)) & 0x3;
  return color;
}

void Gpu::draw_line() {
  set_lcdc_status();
  wy = mmu.read_byte(WIN_Y);
  wx = mmu.read_byte(WIN_X);
  scy = mmu.read_byte(SCY);
  scx = mmu.read_byte(SCX);
  uint8_t curr_line = mmu.read_byte(LY);

  if (!lcd_enable) {
    set_mode(2);
    return;
  }

  for (int i = 0; i < SCREEN_WIDTH; i++) {
    screen[curr_line][i] = draw_bg_pixel(i, curr_line); // draws the background on the current scanline
  }
}

void Gpu::step(uint8_t cycles) {
  mode_clock += cycles;
  switch (mode) {
    case 2:
      if (mode_clock >= MODE_2_CYCLES) {
        set_mode(3);
        mode_clock -= MODE_2_CYCLES;
      }
      break;
    case 3:
      if (mode_clock >= MODE_3_CYCLES) {
        set_mode(0);
        draw_line();
        mode_clock -= MODE_3_CYCLES;
      }
      break;
    case 0:
      if (mode_clock >= MODE_0_CYCLES) {
        mmu.inc_scanline();
        if (mmu.read_byte(LY) == SCREEN_HEIGHT) set_mode(1); 
        else set_mode(2);
        mode_clock -= MODE_0_CYCLES;
      }
      break;
    case 1:
      if (mode_clock >= MODE_1_CYCLES) {
        mmu.inc_scanline();
        if (mmu.read_byte(LY) > 153) {
          mmu.reset_scanline();
          set_mode(2);
        }
        mode_clock -= MODE_1_CYCLES;
      }
      break;
  }
}

void Gpu::set_draw_color(uint8_t color) {
  switch(color) {
    case 0:
      SDL_SetRenderDrawColor(renderer, 155, 188, 15, 255);
     break;
    case 1:
      SDL_SetRenderDrawColor(renderer, 139, 172, 15, 255);
     break;
    case 2:
      SDL_SetRenderDrawColor(renderer, 48, 98, 48, 255);
      break;
    case 3:
      SDL_SetRenderDrawColor(renderer, 15, 56, 15, 255);
      break;
  }
}

void Gpu::render() {
  for (int y = 0; y < SCREEN_HEIGHT; y++) {
    for (int x = 0; x < SCREEN_WIDTH; x++) {
      SDL_Rect pixel = {
        x * SCALE_FACTOR,
        y * SCALE_FACTOR,
        SCALE_FACTOR,
        SCALE_FACTOR
      };
      set_draw_color(screen[y][x]);
      SDL_RenderFillRect(renderer, &pixel);
    }
  }
  SDL_RenderPresent(renderer);
}
