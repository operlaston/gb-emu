#ifndef MEMORY_H
#define MEMORY_H

// timer registers (addresses in memory)
#define DIV_REG (0xFF04) // div timer
#define TIMA_REG (0xFF05) // the address of the timer
#define TMA_REG (0xFF06) // address of the value to reset the timer to
#define TAC_REG (0xFF07) // address of the frequency of the timer

// interrupt registers
#define IF_REG (0xFF0F)
#define IE_REG (0xFFFF)

enum banking_types {
  MBC1,
  MBC2,
  NONE
};

class Memory {
private:
  unsigned char mem[0x10000];
  unsigned char rom[0x200000];
  unsigned char ram_banks[0x8000]; // a ram bank is 0x2000 in size and there are 4 max
  void handle_banking(unsigned short address, unsigned char data);
  unsigned char num_rom_banks; // rom banks are 16KiB in size
  unsigned char num_ram_banks; // ram banks are 8KiB in size
  enum banking_types rom_banking_type;
  unsigned char curr_rom_bank;
  unsigned char curr_ram_bank;
  bool ram_enabled;

public:
  Memory(char *rom_path);
  // use default destructor
  void write_byte(unsigned short address, unsigned char data);
  unsigned char read_byte(unsigned short address) const;
  unsigned short read_word(unsigned short address) const;
  void inc_div();
};

#endif
