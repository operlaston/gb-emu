#include "gpu.hh"
#include "constants.hh"
#include <cstdlib>
#include <cstring>
#include <stdio.h>

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
  win_tile_map_base = get_lcdc_bit(WIN_TILE_MAP) ? 0x9C00 : 0x9800;
  bg_tile_map_base = get_lcdc_bit(BG_TILE_MAP) ? 0x9C00 : 0x9800;
  tile_data_base = get_lcdc_bit(TILE_DATA) ? 0x8000 : 0x9000;
  sprite_height = get_lcdc_bit(OBJ_SIZE) ? 8 : 16;
  sprite_enable = get_lcdc_bit(OBJ_ENABLE) ? 1 : 0;
  bg_win_enable = get_lcdc_bit(BG_WIN_ENABLE) ? 1 : 0;
}

void Gpu::set_mode(uint8_t mode) {
  this->mode = mode;
  mmu.write_byte(LCD_STATUS, mmu.read_byte(LCD_STATUS) | mode);
}

void Gpu::draw_tile_line(uint8_t curr_line) {
  int16_t wx_adjusted = wx - 7;
  uint8_t win_x_end = wx_adjusted >= 0 ? 0xFF : wx_adjusted + 0xFF;
  // use & 0xFF to explicitly state that we want the lower byte; in reality, it's not needed
  uint8_t u_wx_adjusted = wx_adjusted < 0 ? 0 : wx_adjusted & 0xFF; 
  uint16_t tile_map = bg_tile_map_base;

  // retrieve the tile index
  uint8_t pixel_x = (curr_x + scx) & 0xFF; // & 0xFF allows wrapping
  uint8_t pixel_y = (curr_line + scy) & 0xFF;
  if ((pixel_x >= u_wx_adjusted && pixel_x <= win_x_end)
      && pixel_y >= wy
      && win_enable) { // the pixel is within the window and the window is enabled
    // we draw the window pixel instead
    tile_map = win_tile_map_base;
  }
  uint8_t tile_x = pixel_x / 8; // int divide by 8 to get tile x,y
  uint8_t tile_y = pixel_y / 8;
  uint16_t tile_index_addr = tile_map + (tile_y * 32) + tile_x; // each row is 32 tiles
  uint8_t tile_index = mmu.read_byte(tile_index_addr);

  // find the location of tile data using the tile index
  uint16_t tile_addr = tile_data_base + (tile_index * 16); // each tile is 16 bytes
  if (tile_data_base == 0x9000) tile_addr = tile_data_base + ((int8_t)tile_index * 16);

  // retrieve tile data
  uint8_t tile_px = pixel_x & 0x7; // get the pixel x offset within the tile, same as % 8
  uint8_t tile_py = pixel_y & 0x7; // get the pixel y offset within the tile, same as % 8
  uint8_t byte1 = mmu.read_byte(tile_addr + (tile_py * 2)); // apply the y offset to get the correct line 
  uint8_t byte2 = mmu.read_byte(tile_addr + (tile_py * 2) + 1); // each line in the tile is 2 bytes

  // & bytes together to get pixels
  for (int i = tile_px; i <= 7; i++) { // draw a tile at a time
    uint8_t bit = 7 - i;
    uint8_t color_id = (((byte2 >> bit) & 1) << 1) | ((byte1 >> bit) & 1);
    uint8_t palette = mmu.read_byte(0xFF47);
    uint8_t color = (palette >> (color_id * 2)) & 0x3; // extract the color from the palette
    screen[curr_line][curr_x++] = color;
    if (curr_x >= 160) return;
  }
}

void Gpu::draw_sprite_tile_line(int16_t signed_curr_line, int16_t sprite_x, 
                                int16_t sprite_y, uint8_t tile_index, uint8_t flags) {

  uint16_t tile_addr = 0x8000 + tile_index * 16;
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
        screen[screen_y][screen_x] = color;
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

  curr_x = 0;
  // draw bg or window
  while (curr_x < 160) { // it should loop either 20 or 21 times
    draw_tile_line(curr_line);
  }
  // curr_x = 0;

  // draw sprites
  for (int i = OAM_START; i <= OAM_END; i += 4) {
    // retrieve sprite x and y and perform checks to see if they
    // fit on the current scan line
    int16_t signed_curr_line = (int16_t) curr_line;
    int16_t sprite_y = mmu.read_byte(i);
    sprite_y -= 16;
    if (sprite_y + sprite_height <= 0) continue;
    int16_t sprite_y_end = sprite_y + sprite_height - 1;
    if (sprite_y <= signed_curr_line && sprite_y_end >= signed_curr_line) continue;
    int16_t sprite_x = mmu.read_byte(i + 1);
    sprite_x -= 8;
    if (sprite_x == -8 || sprite_x >= SCREEN_WIDTH) continue;

    uint8_t tile_index = mmu.read_byte(i + 2);
    uint8_t flags = mmu.read_byte(i + 3);
    draw_sprite_tile_line(signed_curr_line, sprite_x, sprite_y, tile_index, flags);
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
