#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>

enum banking_types {
  MBC1,
  MBC1_RAM,
  MBC1_RAM_BATTERY,
  MBC2,
  MBC3,
  MBC3_RAM,
  MBC3_RAM_BATTERY,
  NONE
};

class Timer;
class Joypad;
class Gpu;

class Memory {
private:
  unsigned char num_rom_banks; // rom banks are 16KiB in size
  uint32_t ram_size;
  enum banking_types banking_type;
  unsigned char curr_rom_bank;
  unsigned char curr_ram_bank;
  bool mode_flag;
  bool ram_enabled;
  Timer *timer;
  Joypad *joypad;

  void handle_banking(unsigned short address, unsigned char data);
  void dma_transfer(uint8_t);
  bool is_lcd_enabled() const;

public:
  Memory(char *rom_path);
  // use default destructor
  
  void write_byte(unsigned short address, unsigned char data);
  unsigned char read_byte(unsigned short address) const;
  unsigned short read_word(unsigned short address) const;
  void request_interrupt(uint8_t);
  void reset_scanline();
  void reset_lcd_status();
  void inc_scanline();
  void check_lyc_ly();
  uint8_t mbc1_read(uint16_t address) const;
  void set_timer(Timer *t);
  void set_joypad(Joypad *j);
};

#endif
