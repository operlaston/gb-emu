#include "memory.hh"
#include "constants.hh"
#include <cstdio>
#include <iostream>

using namespace std;

Memory::Memory(char *rom_path) {

  FILE *rom_fp = fopen(rom_path, "rb"); // open in read binary mode
  if (rom_fp == NULL) {
    cout << "Failed to load rom with filepath " << rom_path << endl;
    exit(1);
  }

  fseek(rom_fp, 0, SEEK_END);
  unsigned long fsize = ftell(rom_fp); 
  rewind(rom_fp);
  if (fsize > sizeof(cart)) { 
    cout << "rom is too large" << endl;
    fclose(rom_fp);
    rom_fp = NULL;
    exit(1);
  }
  if (fread(cart, 1, fsize, rom_fp) != fsize) { 
    cout << "failed to read rom contents" << endl;
    fclose(rom_fp);
    rom_fp = NULL;
    exit(1);
  }

  fclose(rom_fp);
  rom_fp = NULL;

  if (cart[0x147] <= 3 && cart[0x147] >= 1) { 
    rom_banking_type = MBC1;
  }
  else if (cart[0x147] == 5 || cart[0x147] == 6) { 
    rom_banking_type = MBC2;
  }
  else {
    rom_banking_type = NONE;
  }

  num_rom_banks = 2 << cart[0x148];
  if (cart[0x148] > 6) {
    cout << "invalid byte at 0x148 for rom size" << endl;
    exit(1);
  }

  switch(cart[0x149]) {
    case 0x0:
      num_ram_banks = 0;
      break;
    case 0x2:
      num_ram_banks = 1;
      break;
    case 0x3:
      num_ram_banks = 4;
      break;
    case 0x4:
      num_ram_banks = 16;
      break;
    case 0x5:
      num_ram_banks = 8;
      break;
    
  }

  uint8_t checksum = 0;
  for (uint16_t address = 0x0134; address <= 0x014C; address++) {
    checksum = checksum - cart[address] - 1;
  }
  if (checksum != cart[0x14D]) {
    cout << "checksum did not pass. stopped during init" << endl;
    exit(1);
  }

  curr_rom_bank = 1; // rom bank at 0x4000-0x7fff (default is 1)
  memset(ram_banks, 0, sizeof(ram_banks));
  curr_ram_bank = 0;

  ram_enabled = false;

  // copy rom into memory
  for (unsigned long i = 0; i < 0x8000; i++) {
    mem[i] = cart[i];
  }

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

  printf("initialized memory\n");
}

void Memory::handle_banking(unsigned short address, unsigned char data) {
  if (rom_banking_type == MBC1) {
    if (address < 0x2000) {
      unsigned char lower_bits = data & 0xF;
      if ( lower_bits == 0xA ) {
        ram_enabled = true; //enable ram bank writing
      }
      else{
        ram_enabled = false; //disable ram bank writing
      }
    } 
    else if (address < 0x4000) {
      // TODO: rom bank change

    }
    else if (address < 0x6000){
      // TODO: rom or ram bank change
    }
    else {
      // TODO: do something else
    }
  }
  else if (rom_banking_type == MBC2) {
    if ( address < 0x4000 ) {
      unsigned char lower_bits = data & 0xF;
    // The least significant bit of the upper address byte must be zero to enable/disable cart RAM
      if ((address & 0x10) == 0) { // bit 8 is not set
        if ( lower_bits == 0xA ) {
          ram_enabled = true; //enable ram bank writing
        }
        else{
          ram_enabled = false; //disable ram bank writing
        }
      }
      else { // bit 8 is set
        curr_rom_bank = data & 0xF;
        curr_rom_bank = curr_rom_bank == 0 ? 1 : curr_rom_bank;
      }
    }
  }
}

void Memory::write_byte(unsigned short address, unsigned char data) {

  // remove after testing
  if (address == 0xFF02) {
    char c = read_byte(0xFF01);
    printf("%c", c);
    fflush(stdout);
    data &= ~0x80;
  }

  // 0x0000-0x7FFF is read only
  if (address < 0x8000) {
    handle_banking(address, data);
    return;
  }

  // writing to internal ram also writes to echo ram
  else if (address >= 0xC000 && address <=0xDDFF) {
    mem[address] = data;
    mem[address + 0x2000] = data;
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

  else if (address == DIV_REG) { // 0xFF04 (timer)
    // writing anything to this register resets it to 0
    mem[DIV_REG] = 0;
  }

  else if (address == LY) {
    mem[LY] = 0;
    if (mem[LY] == mem[LYC]) {
      mem[LCD_STATUS] = mem[LCD_STATUS] | (1 << 2);
      if ((mem[LCD_STATUS] >> 6) & 0x1) {
        request_interrupt(STAT_INTER);
      }
    }
    else mem[LCD_STATUS] = mem[LCD_STATUS] & ~(1 << 2);
  }

  else if (address == 0xFF46) { // DMA transfer
    // DMA transfer
    uint16_t xfer_i = data << 8;
    for (int i = 0xFE00; i < 0xFEA0; i++) {
      mem[i] = mem[xfer_i];
      xfer_i++;
    }
  }

  else {
    mem[address] = data;
  }
}


unsigned char Memory::read_byte(unsigned short address) const {
  // reading from ROM bank
  if (address >= 0x4000 && address <= 0x7FFF) {
    unsigned short offset = address - 0x4000;
    return cart[offset + (curr_rom_bank * 0x4000)];
  }

  // reading from RAM bank
  else if (address >= 0xA000 && address <= 0xBFFF) {
    unsigned short offset = address - 0xA000;
    return ram_banks[offset + (curr_ram_bank * 0x2000)];
  }

  return mem[address];
}

// gameboy is little endian
unsigned short Memory::read_word(unsigned short address) const {
  unsigned char lower_byte = read_byte(address);
  unsigned char upper_byte = read_byte(address + 1);
  return (upper_byte << 8) | lower_byte;
}

void Memory::inc_div() {
  mem[DIV_REG]++;
}

void Memory::request_interrupt(uint8_t bit) {
  // IE (interrupt enable): 0xFFFF
  // IF (interrupt flag/requested): 0xFF0F

  write_byte(IF_REG, read_byte(IF_REG) | (1 << bit));
}

void Memory::reset_scanline() {
  mem[LY] = 0;
}

void Memory::inc_scanline() {
  mem[LY]++; 
  if (mem[LY] == mem[LYC]) {
    mem[LCD_STATUS] = mem[LCD_STATUS] | (1 << 2);
    if ((mem[LCD_STATUS] >> 6) & 0x1) {
      request_interrupt(STAT_INTER);
    }
  }
  else mem[LCD_STATUS] = mem[LCD_STATUS] & ~(1 << 2);
}
