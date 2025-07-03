#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>

enum banking_types {
  MBC1,
  MBC1_RAM,
  MBC1_RAM_BATTERY,
  MBC3,
  MBC3_RAM,
  MBC3_RAM_BATTERY,
  NO_BANKING
};

enum ram_sizes {
  NO_BANKS = 0,
  ONE_BANK = 8192,
  FOUR_BANKS = 32768,
  SIXTEEN_BANKS = 131072,
  EIGHT_BANKS = 65536,
};

class Timer;
class Joypad;
class Cpu;

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
  Cpu *cpu;

  uint8_t mbc_read(unsigned short address) const;
  void mbc_write(unsigned short address, unsigned char data);
  void dma_transfer(uint8_t);
  bool is_lcd_enabled() const;

  uint8_t mbc1_read(uint16_t address) const;
  void mbc1_write(uint16_t address, uint8_t data);

  uint8_t mbc3_read(uint16_t address) const;
  void mbc3_write(uint16_t address, uint8_t data);

  

public:
  Memory(char *rom_file);
  // use default destructor
  
  void write_byte(unsigned short address, unsigned char data);
  unsigned char read_byte(unsigned short address) const;
  unsigned short read_word(unsigned short address) const;
  void request_interrupt(uint8_t);
  void reset_scanline();
  void reset_lcd_status();
  void set_scanline(uint8_t ly);
  void inc_scanline();
  void check_lyc_ly();
  void set_timer(Timer *t);
  void set_joypad(Joypad *j);
  void set_cpu(Cpu *cpu);

  uint8_t get_ppu_mode() const;
  void set_ppu_mode(uint8_t mode);
};

#endif
