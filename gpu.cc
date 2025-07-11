#include "gpu.hh"
#include "constants.hh"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdio.h>

using namespace std;

typedef struct {
  uint8_t color;
  int16_t sprite_x;
} sprite_prio_t;

uint32_t screen[SCREEN_HEIGHT][SCREEN_WIDTH];
uint32_t colors[4] = {
    0xFFFFFFFF, // white
    0xAAAAAAFF, // light gray
    0x555555FF, // dark gray
    0x000000FF, // black
};

Gpu::Gpu(Memory &mem) : mmu(mem) {
  mode_clock = 0;
  memset(screen, colors[0], sizeof(screen));
  // mmu.set_ppu_mode(2);
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
}

void Gpu::init_sdl(SDL_Renderer *r, SDL_Texture *t) {
  this->renderer = r;
  this->texture = t;
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
  sprite_height = get_lcdc_bit(OBJ_SIZE) ? 16 : 8;
  bg_win_enable = get_lcdc_bit(BG_WIN_ENABLE) ? 1 : 0;
}

// void Gpu::mmu.set_ppu_mode(uint8_t mode) {
//   this->mode = mode;
//   mmu.write_byte(LCD_STATUS, mmu.read_byte(LCD_STATUS) | mode);
// }

void Gpu::draw_bg_pixel(uint8_t palette) {
  uint16_t tile_map_base = bg_tile_map_base;

  // retrieve the tile address
  uint8_t pixel_x = (x_pos + scx) & 0xFF; // & 0xFF allows wrapping
  uint8_t pixel_y = (curr_line + scy) & 0xFF;
  uint16_t tile_addr = get_tile_addr(pixel_x, pixel_y, tile_map_base);

  // draw the pixel
  draw_pixel(palette, pixel_x, pixel_y, tile_addr);
}

void Gpu::draw_win_pixel(uint8_t palette) {
  uint16_t tile_map_base = win_tile_map_base;

  // retrieve the tile address
  uint8_t win_x = x_pos - (wx - 7);
  uint16_t tile_addr = get_tile_addr(win_x, win_line, tile_map_base);

  // draw the pixel
  draw_pixel(palette, win_x, win_line, tile_addr);
}

uint16_t Gpu::get_tile_addr(uint8_t x, uint8_t y, uint16_t tile_map_base) {
  uint8_t tile_x = x >> 3; // int divide by 8 to get tile x,y
  uint8_t tile_y = y >> 3;
  uint16_t tile_index_addr =
      tile_map_base + (tile_y << 5) + tile_x; // each row is 32 tiles
  uint8_t tile_index = mmu.read_byte(tile_index_addr);

  // find the location of tile data using the tile index
  uint16_t tile_addr =
      tile_data_base + (tile_index << 4); // each tile is 16 bytes
  if (tile_data_base == 0x9000)
    tile_addr = tile_data_base + (((int8_t)tile_index) << 4);
  return tile_addr;
}

void Gpu::draw_pixel(uint8_t palette, uint8_t x, uint8_t y,
                     uint16_t tile_addr) {
  uint8_t byte1 = mmu.read_byte(
      tile_addr +
      ((y & 0x7) << 1)); // apply the y offset to get the correct line
  uint8_t byte2 = mmu.read_byte(tile_addr + ((y & 0x7) << 1) +
                                1); // each line in the tile is 2 bytes

  uint8_t bit = 7 - (x & 0x7);
  uint8_t color_id = (((byte2 >> bit) & 1) << 1) | ((byte1 >> bit) & 1);
  uint8_t color =
      (palette >> (color_id << 1)) & 0x3; // extract the color from the palette
  screen[curr_line][x_pos++] = colors[color];
}

// sprite_x is the pixel to draw's x position relative to the tile (affected by
// x flip) pos_x is the x position where the pixel will be drawn relative to the
// screen (unaffected by x flip)
void Gpu::draw_sprite_pixel(uint8_t palette, uint8_t draw_x, uint8_t draw_y,
                            uint8_t pos_x, uint8_t pos_y, uint16_t tile_addr) {
  uint8_t byte1 = mmu.read_byte(
      tile_addr +
      ((draw_y & 7) << 1)); // apply the y offset to get the correct line
  uint8_t byte2 = mmu.read_byte(tile_addr + ((draw_y & 7) << 1) +
                                1); // each line in the tile is 2 bytes

  uint8_t bit = 7 - draw_x;
  uint8_t color_id = (((byte2 >> bit) & 1) << 1) | ((byte1 >> bit) & 1);
  if (color_id == 0) {
    return;
  }
  uint8_t color =
      (palette >> (color_id << 1)) & 0x3; // extract the color from the palette
  screen[pos_y][pos_x] = colors[color];
}

void Gpu::draw_sprite(sprite_t sprite) {

  bool y_flip = (sprite.flags >> 6) & 1;
  bool x_flip = (sprite.flags >> 5) & 1;
  bool obp1 = (sprite.flags >> 4) & 1;
  bool bg_priority = (sprite.flags >> 7) & 1;
  uint8_t palette = mmu.read_byte(0xFF48); // OBP0 register
  if (obp1)
    palette = mmu.read_byte(0xFF49); // OBP1 register

  uint8_t draw_y = curr_line - sprite.y;
  if (y_flip)
    draw_y = sprite_height - draw_y - 1;

  if (sprite_height == 16) {
    if (draw_y >= 8) {
      sprite.tile_index |= 1;
    } else {
      sprite.tile_index &= 0xFE;
    }
  }
  uint16_t tile_addr =
      0x8000 + (sprite.tile_index << 4); // each tile is 16 bytes

  for (int i = 0; i < 8; i++) {
    if (sprite.x + i >= SCREEN_WIDTH)
      break;
    if (sprite.x + i < 0)
      continue;
    uint8_t draw_x = i;
    if (x_flip)
      draw_x = 7 - draw_x;
    if (screen[curr_line][sprite.x + i] == colors[0] || bg_priority == 0) {
      draw_sprite_pixel(palette, draw_x, draw_y, sprite.x + i, curr_line,
                        tile_addr);
    }
  }
}

void Gpu::draw_sprites() {
  sprite_t sprites[10];
  uint8_t num_sprites = 0;
  for (uint8_t i = 0; i < 40 && num_sprites < 10; i++) { // there are 40 sprites
    uint16_t addr = OAM_START + (i * 4);
    int16_t x = mmu.read_byte(addr + 1) - 8;
    int16_t y = mmu.read_byte(addr) - 16;
    int16_t ly_signed = curr_line & 0x00FF;
    if (ly_signed >= y && ly_signed < y + sprite_height) {
      sprite_t sprite = {
          x, y,
          mmu.read_byte(addr + 2), // tile index
          mmu.read_byte(addr + 3), // flags
      };
      sprites[num_sprites++] = sprite;
    }
  }
  stable_sort(begin(sprites), begin(sprites) + num_sprites,
              [](const sprite_t &a, const sprite_t &b) { return a.x < b.x; });
  // if (curr_line >= 102) {
  //   printf("Line %d: ", curr_line);
  // }
  for (int i = num_sprites - 1; i >= 0; i--) {
    // if (curr_line >= 94 && sprites[i].tile_index == 98) {
    //   printf("Line: %d x: %d y: %d tile: %d flags: %d\n", curr_line,
    //   sprites[i].x, sprites[i].y, sprites[i].tile_index, sprites[i].flags);
    // }
    // if (curr_line >= 102 && curr_line <= 113) {
    //   printf("x: %d y: %d tile: %d flags: %d\n", sprites[i].x, sprites[i].y,
    //   sprites[i].tile_index, sprites[i].flags);
    // }
    draw_sprite(sprites[i]);
  }
}

void Gpu::draw_line() {
  set_lcdc_status();
  wy = mmu.read_byte(WIN_Y);
  wx = mmu.read_byte(WIN_X);
  scy = mmu.read_byte(SCY);
  scx = mmu.read_byte(SCX);
  curr_line = mmu.read_byte(LY);

  // if (!lcd_enable) {
  //   mmu.set_ppu_mode(0);
  //   mmu.reset_scanline();
  //   return;
  // }

  // printf("draw line: %d\n", curr_line);

  if (bg_win_enable) {
    uint8_t palette = mmu.read_byte(0xFF47);

    // printf("drawing bg\n");
    // draw bg
    x_pos = 0;
    while (x_pos < 160) {
      draw_bg_pixel(palette);
    }

    // draw window
    x_pos = wx < 7 ? 0 : (wx - 7);
    if (win_enable && win_line_enable && wx < 167) {
      while (x_pos < 160) {
        draw_win_pixel(palette);
      }
      win_line++;
    }
  }

  if (sprite_enable) {
    draw_sprites();
  }
}

void Gpu::step(uint8_t cycles) {
  bool prev_lcd_enable = lcd_enable;
  set_lcdc_status();
  if (!lcd_enable) {
    if (prev_lcd_enable) {
      mode_clock = 0;
      mmu.set_ppu_mode(0);
      mmu.reset_scanline();
      win_line = 0;
      curr_line = 0;
      for (int i = 0; i < SCREEN_HEIGHT; i++) {
        for (int j = 0; j < SCREEN_WIDTH; j++) {
          screen[i][j] = colors[0];
        }
      }
      render();
    }
    return;
  } else if (!prev_lcd_enable) {
    mmu.check_lyc_ly();
    mmu.set_ppu_mode(2);
  }

  mode_clock += cycles;
  // printf("step. ppu mode: %d\n", mmu.get_ppu_mode());
  switch (mmu.get_ppu_mode()) {
  case 2: // OAM
    if (mode_clock >= MODE_2_CYCLES) {
      if (mmu.read_byte(LY) == wy) {
        win_line_enable = true;
      }
      mmu.set_ppu_mode(3);
      mode_clock -= MODE_2_CYCLES;
    }
    break;
  case 3: // DRAW
    if (mode_clock >= MODE_3_CYCLES) {
      mmu.set_ppu_mode(0);
      draw_line();
      mode_clock -= MODE_3_CYCLES;
      if (get_stat_bit(MODE_0)) {
        mmu.request_interrupt(STAT_INTER);
      }
    }
    break;
  case 0: // HBLANK
    if (mode_clock >= MODE_0_CYCLES) {
      mmu.inc_scanline();
      if (mmu.read_byte(LY) == SCREEN_HEIGHT) {
        render();
        mmu.request_interrupt(VBLANK_INTER);
        mmu.set_ppu_mode(1);
        if (get_stat_bit(MODE_1)) {
          mmu.request_interrupt(STAT_INTER);
        }
      } else {
        mmu.set_ppu_mode(2);
        if (get_stat_bit(MODE_2)) {
          mmu.request_interrupt(STAT_INTER);
        }
      }
      mode_clock -= MODE_0_CYCLES;
    }
    break;
  case 1: // VBLANK
    if (mode_clock >= MODE_1_CYCLES) {
      mmu.inc_scanline();
      win_line++;
      if (mmu.read_byte(LY) > 153) {
        mmu.reset_scanline();
        win_line = 0;
        win_line_enable = false;
        mmu.set_ppu_mode(2);
      }
      mode_clock -= MODE_1_CYCLES;
    }
    break;
  }
}

void Gpu::render() {
  SDL_UpdateTexture(texture, NULL, screen, SCREEN_WIDTH * sizeof(uint32_t));
  SDL_RenderClear(renderer);
  SDL_RenderCopy(renderer, texture, NULL, NULL);
  SDL_RenderPresent(renderer);
}
