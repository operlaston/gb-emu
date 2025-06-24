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
  curr_x = 0;

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

bool Gpu::get_stat_bit(LCD_STAT_BIT bit) {
  uint8_t stat_reg = mmu.read_byte(LCD_STATUS);
  return stat_reg & (1 << bit);
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
  mmu.write_byte(LCD_STATUS, mmu.read_byte(LCD_STATUS) | mode);
}

void Gpu::draw_bg_tile_line(uint8_t curr_line) {
  // retrieve the tile index
  uint8_t pixel_x = (curr_x + scx) & 0xFF; // & 0xFF allows wrapping
  uint8_t pixel_y = (curr_line + scy) & 0xFF;
  uint8_t tile_x = pixel_x >> 3; // int divide by 8 to get tile x,y
  uint8_t tile_y = pixel_y >> 3;
  uint16_t tile_index_addr = bg_tile_map_addr + (tile_y << 5) + tile_x; // each row is 32 tiles
  uint8_t tile_index = mmu.read_byte(tile_index_addr);

  // find the location of tile data using the tile index
  uint16_t tile_addr = tile_data_addr + (tile_index << 4); // each tile is 16 bytes
  if (tile_data_addr == 0x9000) tile_addr = tile_data_addr + ((int8_t)tile_index << 4);

  // retrieve tile data
  uint8_t tile_px = pixel_x & 0x7; // get the pixel x offset within the tile
  uint8_t tile_py = pixel_y & 0x7; // get the pixel y offset within the tile
  uint8_t byte1 = mmu.read_byte(tile_addr + (tile_py >> 1)); // apply the y offset to get the correct line 
  uint8_t byte2 = mmu.read_byte(tile_addr + (tile_py >> 1) + 1); // each line in the tile is 2 bytes

  // & bytes together to get pixels
  for (int i = tile_px; i <= 7; i++) {
    uint8_t bit = 7 - i;
    uint8_t color_id = (((byte2 >> bit) & 1) << 1) | ((byte1 >> bit) & 1);
    uint8_t palette = mmu.read_byte(0xFF47);
    uint8_t color = (palette >> (color_id << 1)) & 0x3; // extract the color from the palette
    screen[curr_line][curr_x++] = color;
  }
  // uint8_t bit = 7 - tile_px;
  // uint8_t color_id = (((byte2 >> bit) & 1) << 1) | ((byte1 >> bit) & 1);
  // uint8_t palette = mmu.read_byte(0xFF47);
  // uint8_t color = (palette >> (color_id << 1)) & 0x3;
  // return color;
}

void Gpu::draw_win_tile_line(uint8_t curr_line) {
  int8_t wx_adjusted = wx - 7;
  if (wx_adjusted >= scx) {

  }
  else {

  }
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

  curr_x = 0;
  // draw bg
  while (curr_x < 160) { // it should loop either 20 or 21 times
    draw_bg_tile_line(curr_line);
  }
  curr_x = 0;
  // if (win_enable && ((curr_line + scy) & 0xFF) >= wy) {
  //   draw_win_tile_line(curr_line);
  // }
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
        if (get_stat_bit(MODE_0)) {
          mmu.request_interrupt(STAT_INTER);
        }
      }
      break;
    case 0:
      if (mode_clock >= MODE_0_CYCLES) {
        mmu.inc_scanline();
        if (mmu.read_byte(LY) == SCREEN_HEIGHT) {
          mmu.request_interrupt(VBLANK_INTER);
          if (get_stat_bit(MODE_1)) {
            mmu.request_interrupt(STAT_INTER);
          }
          set_mode(1); 
        }
        else {
          if (get_stat_bit(MODE_2)) {
            mmu.request_interrupt(STAT_INTER);
          }
          set_mode(2);
        }
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
      SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
     break;
    case 1:
      SDL_SetRenderDrawColor(renderer, 170, 170, 170, 255);
     break;
    case 2:
      SDL_SetRenderDrawColor(renderer, 85, 85, 85, 255);
      break;
    case 3:
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
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
