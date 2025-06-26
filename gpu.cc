#include "gpu.hh"
#include "constants.hh"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdio.h>

using namespace std;

typedef struct {
  uint8_t color;
  int16_t sprite_x;
} sprite_prio_t;

sprite_prio_t sprite_line[SCREEN_WIDTH] = { {4, 0} }; // 4 is not a possible color
uint8_t screen[SCREEN_HEIGHT][SCREEN_WIDTH];

Gpu::Gpu(Memory& mem) : mmu(mem) {
  mode_clock = 0;
  memset(screen, 0, sizeof(screen));
  set_mode(2);
  win_enable = 0;
  sprite_enable = 0;
  lcd_enable = 0;
  bg_win_enable = 0;
  win_tile_map_base = 0;
  bg_tile_map_base = 0;
  tile_data_base = 0;
  sprite_height = 0;
  scx = 0;
  scy = 0;
  wy = 0;
  wx = 0;
  x_pos = 0;
  win_line = 0;

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

  std::cout << "initialized gpu" << std::endl;
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
  win_tile_map_base = get_lcdc_bit(WIN_TILE_MAP) ? 0x9C00 : 0x9800;
  bg_tile_map_base = get_lcdc_bit(BG_TILE_MAP) ? 0x9C00 : 0x9800;
  tile_data_base = get_lcdc_bit(TILE_DATA) ? 0x8000 : 0x9000;
  sprite_height = get_lcdc_bit(OBJ_SIZE) ? 8 : 16;
  bg_win_enable = get_lcdc_bit(BG_WIN_ENABLE) ? 1 : 0;
}

void Gpu::set_mode(uint8_t mode) {
  this->mode = mode;
  mmu.write_byte(LCD_STATUS, mmu.read_byte(LCD_STATUS) | mode);
}

void Gpu::draw_bg_pixel(uint8_t curr_line, uint8_t palette) {
  uint16_t tile_map_base = bg_tile_map_base;

  // retrieve the tile index
  uint8_t pixel_x = (x_pos + scx) & 0xFF; // & 0xFF allows wrapping
  uint8_t pixel_y = (curr_line + scy) & 0xFF;
  // uint8_t tile_x = pixel_x / 8; // int divide by 8 to get tile x,y
  // uint8_t tile_y = pixel_y / 8;
  // uint16_t tile_index_addr = tile_map + (tile_y * 32) + tile_x; // each row is 32 tiles
  // uint8_t tile_index = mmu.read_byte(tile_index_addr);
  //
  // // find the location of tile data using the tile index
  // uint16_t tile_addr = tile_data_base + (tile_index * 16); // each tile is 16 bytes
  // if (tile_data_base == 0x9000) tile_addr = tile_data_base + ((int8_t)tile_index * 16);
  uint16_t tile_addr = get_tile_addr(pixel_x, pixel_y, tile_map_base);

  // retrieve tile data
  uint8_t tile_px = pixel_x & 0x7; // get the pixel x offset within the tile, same as % 8
  uint8_t tile_py = pixel_y & 0x7; // get the pixel y offset within the tile, same as % 8
  uint8_t byte1 = mmu.read_byte(tile_addr + (tile_py * 2)); // apply the y offset to get the correct line 
  uint8_t byte2 = mmu.read_byte(tile_addr + (tile_py * 2) + 1); // each line in the tile is 2 bytes 

  // draw the pixel
  // uint8_t bit = 7 - tile_px;
  // uint8_t color_id = (((byte2 >> bit) & 1) << 1) | ((byte1 >> bit) & 1);
  // // uint8_t palette = mmu.read_byte(0xFF47);
  // uint8_t color = (palette >> (color_id * 2)) & 0x3; // extract the color from the palette
  // screen[curr_line][x_pos++] = color;
  draw_pixel(palette, byte1, byte2, tile_px, curr_line);

}

void Gpu::draw_win_pixel(uint8_t curr_line, uint8_t palette) {
  uint16_t tile_map_base = win_tile_map_base;

  // retrieve the tile index
  uint8_t win_x = x_pos - (wx - 7);

  // uint8_t tile_x = win_x / 8; // int divide by 8 to get tile x,y
  // uint8_t tile_y = win_line / 8;
  // uint16_t tile_index_addr = tile_map + (tile_y * 32) + tile_x; // each row is 32 tiles
  // uint8_t tile_index = mmu.read_byte(tile_index_addr);
  //
  // // find the location of tile data using the tile index
  // uint16_t tile_addr = tile_data_base + (tile_index * 16); // each tile is 16 bytes
  // if (tile_data_base == 0x9000) tile_addr = tile_data_base + (((int8_t)tile_index) * 16);
  uint16_t tile_addr = get_tile_addr(win_x, win_line, tile_map_base);

  // retrieve tile data
  uint8_t tile_px = win_x & 0x7; // get the pixel x offset within the tile, same as % 8
  uint8_t tile_py = win_line & 0x7; // get the pixel y offset within the tile, same as % 8
  uint8_t byte1 = mmu.read_byte(tile_addr + (tile_py * 2)); // apply the y offset to get the correct line 
  uint8_t byte2 = mmu.read_byte(tile_addr + (tile_py * 2) + 1); // each line in the tile is 2 bytes
  
  // draw the pixel
  // uint8_t bit = 7 - tile_px;
  // uint8_t color_id = (((byte2 >> bit) & 1) << 1) | ((byte1 >> bit) & 1);
  // // uint8_t palette = mmu.read_byte(0xFF47);
  // uint8_t color = (palette >> (color_id * 2)) & 0x3; // extract the color from the palette
  // screen[curr_line][x_pos++] = color;
  draw_pixel(palette, byte1, byte2, tile_px, curr_line);
}

uint16_t Gpu::get_tile_addr(uint8_t x, uint8_t y, uint16_t tile_map_base) {
  uint8_t tile_x = x / 8; // int divide by 8 to get tile x,y
  uint8_t tile_y = y / 8;
  uint16_t tile_index_addr = tile_map_base + (tile_y * 32) + tile_x; // each row is 32 tiles
  uint8_t tile_index = mmu.read_byte(tile_index_addr);

  // find the location of tile data using the tile index
  uint16_t tile_addr = tile_data_base + (tile_index * 16); // each tile is 16 bytes
  if (tile_data_base == 0x9000) 
    tile_addr = tile_data_base + (((int8_t)tile_index) * 16);
  return tile_addr;
}

void Gpu::draw_pixel(uint8_t palette, uint8_t byte1, uint8_t byte2,
                     uint8_t tile_px, uint8_t curr_line) {
  uint8_t bit = 7 - tile_px;
  uint8_t color_id = (((byte2 >> bit) & 1) << 1) | ((byte1 >> bit) & 1);
  // uint8_t palette = mmu.read_byte(0xFF47);
  uint8_t color = (palette >> (color_id * 2)) & 0x3; // extract the color from the palette
  screen[curr_line][x_pos++] = color; 
}

void Gpu::draw_sprite_tile_line(int16_t signed_curr_line, int16_t sprite_x, 
                                int16_t sprite_y, uint8_t tile_index, uint8_t flags) {

  if (sprite_x <= -8 || sprite_x >= SCREEN_WIDTH) return;

  uint16_t tile_addr = 0x8000 + (tile_index * 16);
  uint8_t pixel_y_rel = (signed_curr_line - sprite_y) & 0xFF; // y value of the pixel relative to inside the tile
  if ((flags >> 6) & 1) { // y-flip
    pixel_y_rel = (sprite_height - 1) - pixel_y_rel;
  }
  uint8_t byte1 = mmu.read_byte(tile_addr + (pixel_y_rel * 2));
  uint8_t byte2 = mmu.read_byte(tile_addr + (pixel_y_rel * 2) + 1);
  uint8_t obj_to_bg_priority = (flags >> 7) & 1;
  for (int i = 0; i < 8; i++) {
    if (sprite_x + i < 0) {
      continue;
    }
    uint8_t pixel_x_rel = i;
    if ((flags >> 5) & 1) { // x-flip
      pixel_x_rel = 7 - i;
    }
    uint8_t bit = 7 - pixel_x_rel;
    uint8_t color_id = (((byte2 >> bit) & 1) << 1) | ((byte1 >> bit) & 1);
    if (color_id != 0) {
      // match color id with palette
      uint8_t palette = mmu.read_byte(0xFF48); // OBP0 register
      if ((flags >> 4) & 1) {
        palette = mmu.read_byte(0xFF49);
      }
      uint8_t color = (palette >> (color_id * 2)) & 0x3;
      uint8_t screen_y = (uint8_t) signed_curr_line;
      uint8_t screen_x = sprite_x + i;
      if (screen[screen_y][screen_x] == 0 || obj_to_bg_priority == 0) {
        // current sprite_x is guaranteed to be lower OAM index so we just compare x vals
        if(sprite_line[screen_x].color == 4 || sprite_x <= sprite_line[screen_x].sprite_x) {
          screen[screen_y][screen_x] = color;
          sprite_line[screen_x] = {color, sprite_x};
        }
        // even though lower priority, the higher priority sprite is transparent
        else if (sprite_line[screen_x].color == 0) {
          screen[screen_y][screen_x] = color;
          sprite_line[screen_x] = {color, sprite_x};
        }
      }
    }
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

  if (bg_win_enable) {
    uint8_t palette = mmu.read_byte(0xFF47);
    x_pos = 0;
    // draw bg
    while (x_pos < 160) { 
      draw_bg_pixel(curr_line, palette);
    }
    x_pos = wx < 7 ? 0 : (wx - 7);
    // draw window
    if (win_enable && win_line_enable && wx < 167) {
      while (x_pos < 160) {
        draw_win_pixel(curr_line, palette);
      }
      win_line++;
    }
  }

  // if (sprite_enable) {
  //   // draw sprites
  //   int num_sprites = 0;
  //   sprite_t sprites[10];
  //   int16_t signed_curr_line = (int16_t) curr_line;
  //   for (int i = OAM_START; i <= OAM_END && num_sprites < 10; i += 4) {
  //     // retrieve sprite x and y and perform checks to see if they
  //     // fit on the current scan line
  //     int16_t sprite_y = mmu.read_byte(i);
  //     sprite_y -= 16;
  //     if (sprite_y + sprite_height <= 0) continue;
  //     int16_t sprite_y_end = sprite_y + sprite_height - 1;
  //     if (sprite_y > signed_curr_line || sprite_y_end < signed_curr_line) continue;
  //     int16_t sprite_x = mmu.read_byte(i + 1);
  //     sprite_x -= 8;
  //     // if (sprite_x <= -8 || sprite_x >= SCREEN_WIDTH) continue;
  //
  //     uint8_t tile_index = mmu.read_byte(i + 2);
  //     if (sprite_height == 16) {
  //       tile_index &= 0xFE;
  //     }
  //     uint8_t flags = mmu.read_byte(i + 3);
  //     sprite_t sprite = {
  //       sprite_x,
  //       sprite_y,
  //       tile_index,
  //       flags,
  //     };
  //     sprites[num_sprites++] = sprite;
  //     //draw_sprite_tile_line(signed_curr_line, sprite_x, sprite_y, tile_index, flags);
  //   }
  //
  //   for (int i = num_sprites - 1; i >= 0; i--) {
  //     sprite_t sprite = sprites[i];
  //     draw_sprite_tile_line(signed_curr_line, sprite.x, sprite.y, sprite.tile_index,
  //                           sprite.flags);
  //   }
  // }
}

void Gpu::step(uint8_t cycles) {
  mode_clock += cycles;
  switch (mode) {
    case 2:
      if (mode_clock >= MODE_2_CYCLES) {
        if (mmu.read_byte(LY) == wy) {
          win_line_enable = true;
        }
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
        win_line++;
        if (mmu.read_byte(LY) > 153) {
          mmu.reset_scanline();
          win_line = 0;
          win_line_enable = false;
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
