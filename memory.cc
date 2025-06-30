#include "memory.hh"
#include "constants.hh"
#include "timer.hh"
#include "joypad.hh"
#include <cstdio>
#include <iostream>
#include <istream>

using namespace std;

static unsigned char mem[0x10000];
static unsigned char cart[0x200000];
static unsigned char ram_banks[0x8000]; // a ram bank is 0x2000 in size and there are 4 max

Memory::Memory(char *rom_path){

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

  switch(cart[0x147]) {
    case 0:
      banking_type = NONE;
      printf("No memory banking\n");
      break;
    case 0x1:
      banking_type = MBC1;
      printf("MBC1\n");
      break;
    case 0x2:
      banking_type = MBC1_RAM;
      break;
    case 0x3:
      banking_type = MBC1_RAM_BATTERY;
      break;
    case 0x5:
      banking_type = MBC2;
      break;
    case 0x11:
      banking_type = MBC3;
      break;
    case 0x12:
      banking_type = MBC3_RAM;
      break;
    case 0x13:
      banking_type = MBC3_RAM_BATTERY;
      break;
    default:
      banking_type = NONE;
  }

  num_rom_banks = 2 << cart[0x148];
  if (cart[0x148] > 6) {
    cout << "invalid byte at 0x148 for rom size" << endl;
    exit(1);
  }

  switch(cart[0x149]) {
    case 0x0:
      ram_size = 0;
      break;
    case 0x1:
      ram_size = 2048;
      break;
    case 0x2:
      ram_size = 8192;
      break;
    case 0x3:
      ram_size = 32768;
      break;
    case 0x4:
      ram_size = 131072;
      break;
    case 0x5:
      ram_size = 65536;
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

  // reset joypad
  mem[0xFF00] = 0x0F;

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

void Memory::set_timer(Timer *t) {
  timer = t;
}

void Memory::set_joypad(Joypad *j) {
  joypad = j;
}

uint8_t Memory::mbc1_read(uint16_t address) const {
  if (address <= 0x3FFF) {
    return cart[address];
  }

  // reading from ROM bank
  else if (address >= 0x4000 && address <= 0x7FFF) {
    return cart[(address - 0x4000) + (curr_rom_bank * 0x4000)];
  }

  // reading from RAM bank
  else if (address >= 0xA000 && address <= 0xBFFF) {
    if (ram_enabled) {
      return ram_banks[(address - 0xA000) + (curr_ram_bank * 0x2000)]; 
    }
    else return 0xFF;
  }

  // this statement should be unreachable in theory
  return 0;
}

void Memory::handle_banking(unsigned short address, unsigned char data) {
  if (banking_type == MBC1 || banking_type == MBC1_RAM || 
      banking_type == MBC1_RAM_BATTERY) {
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
      uint8_t mask = num_rom_banks >= 32 ? 31 : num_rom_banks - 1;
      if (data == 0) curr_rom_bank = 1;
      else curr_rom_bank = data & mask;
    }
    else if (address < 0x6000){
      // TODO: rom or ram bank change
      curr_ram_bank = data & 0x3;
    }
    else if (address < 0x8000) {
      mode_flag = data & 1;
    }
    else {
      // TODO: do something else
      if (ram_enabled) {
        if(ram_size <= 8192) {
          ram_banks[(address - 0xA000) % ram_size] = data;
        }
        else if (mode_flag) {
          ram_banks[0x2000 * curr_ram_bank + (address - 0xA000)] = data;
        }
        else {
          ram_banks[address - 0xA000] = data;
        }
      }
    }
  }
  else if (banking_type == MBC2) {
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
  // if (address == 0xFF02) {
  //   char c = read_byte(0xFF01);
  //   printf("%c", c);
  //   fflush(stdout);
  //   data &= ~0x80;
  // }
  //
  // if (address == 0xFFFF) {
  //   printf("IE flag is: %d\n", mem[address]);
  //   printf("Setting IE flag to: %d\n", data);
  // }
  // if (address == 0xFF0F) {
  //   printf("IF flag is: %d\n", mem[address]);
  //   printf("Setting IF flag to: %d\n", data);
  // }

  // 0x0000-0x7FFF is read only
  if (address < 0x8000 || (address >= 0xA000 && address < 0xC000)) {
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

  else if (address == 0xFF00) { // joypad register
    joypad->set_joypad_state(data);
  }

  else if (address >= DIV_REG && address <= TAC_REG) {
    timer->timer_write(address, data);
  }

  else if (address == LY) {
    mem[LY] = 0;
    check_lyc_ly();
  }

  else if (address == LYC) {
    mem[LYC] = data;
    check_lyc_ly();
  }

  else if (address == 0xFF46) { // DMA transfer
    // DMA transfer
    dma_transfer(data);
  }

  else {
    mem[address] = data;
  }
}


unsigned char Memory::read_byte(unsigned short address) const {
  // reading from ROM bank
  if (address >= 0x4000 && address <= 0x7FFF) {
    return cart[(address - 0x4000) + (curr_rom_bank * 0x4000)];
  }

  // reading from RAM bank
  else if (address >= 0xA000 && address <= 0xBFFF) {
    if (banking_type == MBC1 || banking_type == MBC1_RAM || banking_type == MBC1_RAM_BATTERY) {
      return mbc1_read(address);
    }
    return ram_banks[(address - 0xA000) + (curr_ram_bank * 0x2000)];
  }

  else if (address >= DIV_REG && address <= TAC_REG) {
    return timer->timer_read(address);
  }

  else if (address == 0xFF00) { // JOYPAD REGISTER
    // printf("read %d from joypad\n", joypad->get_joypad_state());
    // mem[0xFF00] = joypad->get_joypad_state();
    return joypad->get_joypad_state();
  }

  else if (address == LCD_STATUS) {
    // return 0;
    return is_lcd_enabled() ? mem[LCD_STATUS] : 0;
  }

  return mem[address];
}

// gameboy is little endian
unsigned short Memory::read_word(unsigned short address) const {
  unsigned char lower_byte = read_byte(address);
  unsigned char upper_byte = read_byte(address + 1);
  return (upper_byte << 8) | lower_byte;
}

void Memory::request_interrupt(uint8_t bit) {
  // IE (interrupt enable): 0xFFFF
  // IF (interrupt flag/requested): 0xFF0F
  // printf("requested %d\n", bit);
  write_byte(IF_REG, read_byte(IF_REG) | (1 << bit));
}

void Memory::reset_scanline() {
  mem[LY] = 0;
}

void Memory::inc_scanline() {
  mem[LY]++; 
  check_lyc_ly();
}

void Memory::check_lyc_ly() {
  if (mem[LY] == mem[LYC]) {
    mem[LCD_STATUS] = mem[LCD_STATUS] | (1 << 2);
    if ((mem[LCD_STATUS] >> 6) & 0x1) {
      request_interrupt(STAT_INTER);
    }
  }
  else mem[LCD_STATUS] = mem[LCD_STATUS] & ~(1 << 2);
}

void Memory::dma_transfer(uint8_t data) {
  uint16_t xfer_i = data << 8;
  for (int i = 0xFE00; i < 0xFEA0; i++) {
    mem[i] = mem[xfer_i];
    xfer_i++;
  }
}

void Memory::reset_lcd_status() {
  mem[LCD_STATUS] = 0;
}

bool Memory::is_lcd_enabled() const {
  return (mem[LCD_STATUS] >> 7) & 1;
}
