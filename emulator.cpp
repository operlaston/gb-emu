#include "emulator.h"
using namespace std;

Emulator::Emulator(char *rom_path) {
  // initialize program counter, stack pointer, registers
  pc = 0x100;
  sp = 0xFFFE;
  AF.reg = 0x01B0;
  BC.reg = 0x0013;
  DE.reg = 0x00D8;
  HL.reg = 0x014D;

  // set special rom registers
  mem[0xFF05] = 0x00;
  mem[0xFF06] = 0x00;
  mem[0xFF07] = 0x00;
  mem[0xFF10] = 0x80;
  mem[0xFF11] = 0xBF;
  mem[0xFF12] = 0xF3;
  mem[0xFF14] = 0xBF;
  mem[0xFF16] = 0x3F;
  mem[0xFF17] = 0x00;
  mem[0xFF19] = 0xBF;
  mem[0xFF1A] = 0x7F;
  mem[0xFF1B] = 0xFF;
  mem[0xFF1C] = 0x9F;
  mem[0xFF1E] = 0xBF;
  mem[0xFF20] = 0xFF;
  mem[0xFF21] = 0x00;
  mem[0xFF22] = 0x00;
  mem[0xFF23] = 0xBF;
  mem[0xFF24] = 0x77;
  mem[0xFF25] = 0xF3;
  mem[0xFF26] = 0xF1;
  mem[0xFF40] = 0x91;
  mem[0xFF42] = 0x00;
  mem[0xFF43] = 0x00;
  mem[0xFF45] = 0x00;
  mem[0xFF47] = 0xFC;
  mem[0xFF48] = 0xFF;
  mem[0xFF49] = 0xFF;
  mem[0xFF4A] = 0x00;
  mem[0xFF4B] = 0x00;
  mem[0xFFFF] = 0x00;

  // set screen
  memset(screen, 0, sizeof(screen));

  FILE *rom_fp = fopen(rom_path, "rb"); // open in read binary mode
  if (rom_fp == NULL) {
    cout << "Failed to load rom with filepath " << rom_path << endl;
    exit(1);
  }

  fseek(rom_fp, 0, SEEK_END);
  unsigned long fsize = ftell(rom_fp); 
  rewind(rom_fp);
  if (fsize > sizeof(cartridge_memory)) {
    cout << "rom is too large" << endl;
    fclose(rom_fp);
    rom_fp = NULL;
    exit(1);
  }
  if (fread(cartridge_memory, 1, fsize, rom_fp) != fsize) {
    cout << "failed to read rom contents" << endl;
    fclose(rom_fp);
    rom_fp = NULL;
    exit(1);
  }

  if (cartridge_memory[0x147] <= 3 && cartridge_memory[0x147] >= 1) {
    rom_banking_type = MBC1;
  }
  else if (cartridge_memory[0x147] == 5 || cartridge_memory[0x147] == 6) {
    rom_banking_type = MBC2;
  }
  else {
    rom_banking_type = NONE;
  }

  curr_rom_bank = 1; // rom bank at 0x4000-0x7fff (default is 1)
  memset(ram_banks, 0, sizeof(ram_banks));
  curr_ram_bank = 0;

  fclose(rom_fp);
  rom_fp = NULL;
}

Emulator::~Emulator() {

}

unsigned char Emulator::ReadMemory(unsigned short address) const {
  // reading from ROM bank
  if (address >= 0x4000 && address <= 0x7FFF) {
    unsigned short offset = address - 0x4000;
    return mem[offset + (curr_rom_bank * 0x4000)];
  }

  // reading from RAM bank
  else if (address >= 0xA000 && address <= 0xBFFF) {
    unsigned short offset = address - 0xA000;
    return ram_banks[offset + (curr_ram_bank * 0x2000)];
  }
  
  return mem[address];
}

void Emulator::WriteMemory(unsigned short address, unsigned char data) {
  // 0x0000-0x7FFF is read only
  if (address < 0x8000) {
    return;
  }

  // writing to echo ram also writes to ram
  else if (address >= 0xE000 && address <= 0xFDFF) {
    mem[address] = data;
    mem[address - 0x2000] = data;
  }
  
  // this memory is not usable
  else if (address >= 0xFEA0 && address <= 0xFEFF) {
    return;
  }

  else {
    mem[address] = data;
  }
}

void Emulator::update() {
  // max cycles per frame (60 frames per second)
  const int MAXCYCLES = CYCLES_PER_SECOND / 60;
  int cycles_this_update = 0;
  while (cycles_this_update < MAXCYCLES) {
    // perform a cycle
    // update timers
    // update graphics
    // do interrupts
  }
  // render the screen
}
