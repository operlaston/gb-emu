#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>

enum banking_types {
  MBC1,
  MBC2,
  NONE
};

class Timer;

class Memory {
private:
  unsigned char num_rom_banks; // rom banks are 16KiB in size
  unsigned char num_ram_banks; // ram banks are 8KiB in size
  enum banking_types rom_banking_type;
  unsigned char curr_rom_bank;
  unsigned char curr_ram_bank;
  bool ram_enabled;
  Timer *timer;

  void handle_banking(unsigned short address, unsigned char data);
  void dma_transfer(uint8_t);

public:
  Memory(char *rom_path);
  // use default destructor
  
  void write_byte(unsigned short address, unsigned char data);
  unsigned char read_byte(unsigned short address) const;
  unsigned short read_word(unsigned short address) const;
  void request_interrupt(uint8_t);
  void reset_scanline();
  void inc_scanline();
  void check_lyc_ly();
  void set_timer(Timer *t);
};

#endif
