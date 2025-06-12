#ifndef GAMEBOY_H
#define GAMEBOY_H
#include <cinttypes>
#include <string>
#include <cstring>
#include <stdio.h>
#include <iostream>
#include <stdint.h>

#define FLAG_Z (7) // zero flag
#define FLAG_S (6) // subtract flag
#define FLAG_H (5) // half carry flag
#define FLAG_C (4) // carry flag
#define CYCLES_PER_SECOND (4194304)

enum banking_types {
  MBC1,
  MBC2,
  NONE
};

typedef enum {
  REG_A,
  REG_B,
  REG_C,
  REG_D,
  REG_E,
  REG_F,
  REG_H,
  REG_L,
  REG_AF,
  REG_BC,
  REG_DE,
  REG_HL
} REGISTER;

// typedef struct {
//   unsigned char operand_size;
//   std::function<void()> execute_function;
// } Instruction;

union reg_t{
  unsigned char lo;
  unsigned char hi;
  unsigned short reg;
};

class Emulator { 
  unsigned char mem[0x10000];
  unsigned char rom[0x200000];
  unsigned char num_rom_banks; // rom banks are 16KiB in size
  unsigned char num_ram_banks; // ram banks are 8KiB in size
  enum banking_types rom_banking_type;
  unsigned char curr_rom_bank;
  unsigned char curr_ram_bank;
  bool ram_enabled;
  unsigned char ram_banks[0x8000]; // a ram bank is 0x2000 in size and there are 4 max
  std::function<void()> opcode_table[256];
  std::function<void()> cb_table[256];


  // first letter is high and second is low
  // e.g. for AF, the higher half is A and the lower half is F
  // ^^ for little endian systems
  reg_t AF; 
  reg_t BC;
  reg_t DE;
  reg_t HL;

  unsigned short sp;
  unsigned short pc;

  unsigned char screen[160][144][3]; // r/g/b for color


  void handle_banking(unsigned short address, unsigned char data);
  void write_byte(unsigned short address, unsigned char data);
  unsigned char read_byte(unsigned short address) const;
  unsigned short read_short(unsigned short address) const;
  unsigned char *find_r8(REGISTER);
  unsigned short *find_r16(REGISTER);
  void cycle();
  void ld_r8_r8(REGISTER, REGISTER);
  void ld_r8_n8(REGISTER);
  void ld_r16_n16(REGISTER);
  void ld_hl_r8(REGISTER);
  void ld_hl_n8();
  void ld_r8_hl(REGISTER);
  void ld_r16_a(REGISTER);
  void ld_n16_a();
  void ldh_n16_a();
  void ldh_c_a();
  void ld_a_n16();
  void ldh_a_n16();
  void ldh_a_c();
  void ld_hli_a();
  void ld_hld_a();
  void ld_a_hld();
  void ld_a_hli();
  void ld_sp_n16();
  void ld_n16_sp();
  void ld_hl_sp_e8();
  void ld_sp_hl();
  void nop();

public: 
  Emulator(char *rom_path);
  ~Emulator();
  void update();
};

#endif
