#ifndef GAMEBOY_H
#define GAMEBOY_H
#include <string>
#include <cstring>
#include <stdio.h>
#include <iostream>

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

class Emulator { 
  unsigned char mem[0x10000];
  unsigned char cartridge_memory[0x200000];

  union register_t{
    unsigned char lo;
    unsigned char hi;
    unsigned short reg;
  };

  register_t AF;
  register_t BC;
  register_t DE;
  register_t HL;

  unsigned short sp;
  unsigned short pc;

  unsigned char screen[160][144][3]; // r/g/b for color


  enum banking_types rom_banking_type;

public: 
  Emulator(char *rom_path);
  ~Emulator();
  void update();
  void WriteMemory(unsigned short address, unsigned char data);
  unsigned char ReadMemory(unsigned short address);
};

#endif
