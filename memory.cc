#include "memory.hh"
#include "constants.hh"
#include "timer.hh"
#include "joypad.hh"
#include "cpu.hh"
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

static unsigned char mem[0x10000];
static unsigned char cart[0x200000];
static unsigned char ram_banks[0x8000]; // a ram bank is 0x2000 in size and there are 4 max
static unsigned char boot_rom[0x100] = {
  0x31, 0xFE, 0xFF, 0xAF, 0x21, 0xFF, 0x9F, 0x32, 0xCB, 0x7C, 0x20, 0xFB, 0x21, 0x26, 0xFF, 0x0E,
  0x11, 0x3E, 0x80, 0x32, 0xE2, 0x0C, 0x3E, 0xF3, 0xE2, 0x32, 0x3E, 0x77, 0x77, 0x3E, 0xFC, 0xE0,
  0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1A, 0xCD, 0x95, 0x00, 0xCD, 0x96, 0x00, 0x13, 0x7B,
  0xFE, 0x34, 0x20, 0xF3, 0x11, 0xD8, 0x00, 0x06, 0x08, 0x1A, 0x13, 0x22, 0x23, 0x05, 0x20, 0xF9,
  0x3E, 0x19, 0xEA, 0x10, 0x99, 0x21, 0x2F, 0x99, 0x0E, 0x0C, 0x3D, 0x28, 0x08, 0x32, 0x0D, 0x20,
  0xF9, 0x2E, 0x0F, 0x18, 0xF3, 0x67, 0x3E, 0x64, 0x57, 0xE0, 0x42, 0x3E, 0x91, 0xE0, 0x40, 0x04,
  0x1E, 0x02, 0x0E, 0x0C, 0xF0, 0x44, 0xFE, 0x90, 0x20, 0xFA, 0x0D, 0x20, 0xF7, 0x1D, 0x20, 0xF2,
  0x0E, 0x13, 0x24, 0x7C, 0x1E, 0x83, 0xFE, 0x62, 0x28, 0x06, 0x1E, 0xC1, 0xFE, 0x64, 0x20, 0x06,
  0x7B, 0xE2, 0x0C, 0x3E, 0x87, 0xE2, 0xF0, 0x42, 0x90, 0xE0, 0x42, 0x15, 0x20, 0xD2, 0x05, 0x20,
  0x4F, 0x16, 0x20, 0x18, 0xCB, 0x4F, 0x06, 0x04, 0xC5, 0xCB, 0x11, 0x17, 0xC1, 0xCB, 0x11, 0x17,
  0x05, 0x20, 0xF5, 0x22, 0x23, 0x22, 0x23, 0xC9, 0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
  0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D, 0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
  0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99, 0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
  0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E, 0x3C, 0x42, 0xB9, 0xA5, 0xB9, 0xA5, 0x42, 0x3C,
  0x21, 0x04, 0x01, 0x11, 0xA8, 0x00, 0x1A, 0x13, 0xBE, 0x20, 0xFE, 0x23, 0x7D, 0xFE, 0x34, 0x20,
  0xF5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xFB, 0x86, 0x20, 0xFE, 0x3E, 0x01, 0xE0, 0x50
};

Memory::Memory(char *rom_file){
  file_name = rom_file;

  FILE *rom_fp = fopen(rom_file, "rb"); // open in read binary mode
  if (rom_fp == NULL) {
    std::cout << "Failed to load rom with filepath " << rom_file << std::endl;
    exit(1);
  }

  fseek(rom_fp, 0, SEEK_END);
  unsigned long fsize = ftell(rom_fp); 
  rewind(rom_fp);
  if (fsize > sizeof(cart)) { 
    std::cout << "rom is too large" << std::endl;
    fclose(rom_fp);
    rom_fp = NULL;
    exit(1);
  }
  if (fread(cart, 1, fsize, rom_fp) != fsize) { 
    std::cout << "failed to read rom contents" << std::endl;
    fclose(rom_fp);
    rom_fp = NULL;
    exit(1);
  }

  fclose(rom_fp);
  rom_fp = NULL;


  switch(cart[0x147]) {
    case 0:
      banking_type = NO_BANKING;
      printf("Banking Type: NONE\n");
      break;
    case 0x1:
      banking_type = MBC1;
      printf("Banking Type: MBC1\n");
      break;
    case 0x2:
      banking_type = MBC1_RAM;
      printf("Banking Type: MBC1 + RAM\n");
      break;
    case 0x3:
      banking_type = MBC1_RAM_BATTERY;
      printf("Banking Type: MBC1 + RAM + BATTERY\n");
      break;
    case 0x11:
      banking_type = MBC3;
      printf("Banking Type: MBC3\n");
      break;
    case 0x12:
      banking_type = MBC3_RAM;
      printf("Banking Type: MBC3 + RAM\n");
      break;
    case 0x13:
      banking_type = MBC3_RAM_BATTERY;
      printf("Banking Type: MBC3 + RAM + BATTERY\n");
      break;
    default:
      printf("This MBC type is not supported. Only the following \
              banking types are supported: MBC1, MBC1+RAM, MBC1+RAM+BATTERY, \
              MBC3, MBC3+RAM, MBC3+RAM+BATTERY, NO BANKING (without RAM or BATTERY)\n");
      exit(1);
      break;
  }

  num_rom_banks = 2 << cart[0x148];
  if (cart[0x148] > 6) {
    std::cout << "invalid byte at 0x148 for rom size" << std::endl;
    exit(1);
  }

  printf("Number of rom banks: %d\n", num_rom_banks);

  switch(cart[0x149]) {
    case 0x0:
      ram_size = NO_BANKS;
      break;
    case 0x1:
      ram_size = NO_BANKS; // unused
      break;
    case 0x2:
      ram_size = ONE_BANK;
      break;
    case 0x3:
      ram_size = FOUR_BANKS;
      break;
    case 0x4:
      ram_size = SIXTEEN_BANKS;
      break;
    case 0x5:
      ram_size = EIGHT_BANKS;
      break;
    
  }

  printf("RAM Size: %d\n", ram_size);

  uint8_t checksum = 0;
  for (uint16_t address = 0x0134; address <= 0x014C; address++) {
    checksum = checksum - cart[address] - 1;
  }
  if (checksum != cart[0x14D]) {
    std::cout << "checksum did not pass. stopped during init" << std::endl;
    exit(1);
  }

  memset(ram_banks, 0, sizeof(ram_banks));
  if (banking_type == MBC1_RAM_BATTERY || banking_type == MBC3_RAM_BATTERY) {
    std::string save_file = file_name + ".sav";
    int save_fd = open(save_file.c_str(), O_RDONLY);
    if (save_fd >= 0) {
      int bytes_read = read(save_fd, ram_banks, ram_size);
      if (bytes_read < 0) {
        std::cout << "Couldn't read from save file." << std::endl;
        std::cout << "Starting boot anyway." << std::endl;
      }
      else {
        std::cout << bytes_read << " bytes were loaded into RAM." << std::endl;
      }
      close(save_fd);
    }
  }
  curr_rom_bank = 1; // rom bank at 0x4000-0x7fff (default is 1)
  curr_ram_bank = 0;

  ram_enabled = false;
  mode_flag = false;

  // reset joypad
  mem[0xFF00] = 0xFF;

  // special io regs
  // enable when boot is disabled
  // mem[0xFF05] = 0x00;
  // mem[0xFF06] = 0x00;
  // mem[0xFF07] = 0x00;
  // mem[0xFF10] = 0x80;
  // mem[0xFF11] = 0xBF;
  // mem[0xFF12] = 0xF3;
  // mem[0xFF14] = 0xBF;
  // mem[0xFF16] = 0x3F;
  // mem[0xFF17] = 0x00;
  // mem[0xFF19] = 0xBF;
  // mem[0xFF1A] = 0x7F;
  // mem[0xFF1B] = 0xFF;
  // mem[0xFF1C] = 0x9F;
  // mem[0xFF1E] = 0xBF;
  // mem[0xFF20] = 0xFF;
  // mem[0xFF21] = 0x00;
  // mem[0xFF22] = 0x00;
  // mem[0xFF23] = 0xBF;
  // mem[0xFF24] = 0x77;
  // mem[0xFF25] = 0xF3;
  // mem[0xFF26] = 0xF1;
  // mem[0xFF40] = 0x91;
  // mem[0xFF42] = 0x00;
  // mem[0xFF43] = 0x00;
  // mem[0xFF45] = 0x00;
  // mem[0xFF47] = 0xFC;
  // mem[0xFF48] = 0xFF;
  // mem[0xFF49] = 0xFF;
  // mem[0xFF4A] = 0x00;
  // mem[0xFF4B] = 0x00;
  // mem[0xFFFF] = 0x00;
}

void Memory::set_timer(Timer *t) {
  timer = t;
}

void Memory::set_joypad(Joypad *j) {
  joypad = j;
}

void Memory::set_cpu(Cpu *cpu) {
  this->cpu = cpu;
}

int Memory::save_ram() {
  if (banking_type != MBC1_RAM_BATTERY && banking_type != MBC3_RAM_BATTERY) {
    return 0;
  }
  std::string save_file = file_name + ".sav";
  int save_fd = open(save_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (save_fd < 0) {
    std::cout << "Error: could not open save file for writing." << std::endl;
    return -1;
  }
  int bytes_written = write(save_fd, ram_banks, ram_size);
  close(save_fd);
  if (bytes_written < 0) {
    std::cout << "An error occurred. The game could not be saved." << std::endl;
    return -1;
  }
  std::cout << bytes_written << " bytes of RAM were saved." << std::endl;
  return 0;
}

uint8_t Memory::mbc1_read(uint16_t address) const {
  if (address <= 0x3FFF) {
    if (mode_flag && num_rom_banks > 32) {
      uint8_t mask = 0b01100000;
      if (num_rom_banks == 64) {
        mask = 0b00100000;
      }
      return cart[address + (0x4000 * (mask & curr_rom_bank))];
    }
    return cart[address];
  }

  // reading from ROM bank
  else if (address >= 0x4000 && address <= 0x7FFF) {
    // printf("curr rom bank: %d\n", curr_rom_bank);
    // uint32_t offset = 0x4000 * curr_rom_bank;
    return cart[(address - 0x4000) + (0x4000 * curr_rom_bank)];
  }

  // reading from RAM bank
  else if (address >= 0xA000 && address <= 0xBFFF) {
    // printf("attempted ram read\n");
    if (ram_enabled) {
      if (mode_flag) {
        uint32_t offset = 0x2000 * curr_ram_bank;
        return ram_banks[(address - 0xA000) + offset]; 
      }
      return ram_banks[address - 0xA000];
    }
    else return 0xFF;
  }

  // this statement should be unreachable in theory
  return 0xFF;
}

void Memory::mbc1_write(unsigned short address, unsigned char data) {
  if (address < 0x2000) {
    ram_enabled = (data & 0xF) == 0xA;
  } 
  else if (address < 0x4000) {
    uint8_t mask = num_rom_banks >= 32 ? 31 : num_rom_banks - 1;
    if ((data & 0b11111) == 0) curr_rom_bank = 1;
    else curr_rom_bank = data & mask;
  }
  else if (address < 0x6000){
    if (mode_flag) {
      if (ram_size == FOUR_BANKS) {
        curr_ram_bank = data & 0x3;
      }
      if (num_rom_banks > 32) {
        curr_rom_bank = (curr_rom_bank & 0b00011111) | ((data & 3) << 5);
        if (num_rom_banks == 64) {
          curr_rom_bank &= (1 << 6); // mask bit 6 off if not needed
        }
      } 
    }
  }
  else if (address < 0x8000) {
    mode_flag = data & 1;
  }
  else {
    // printf("attempted ram write\n");
    if (ram_enabled) {
      if (mode_flag) {
        ram_banks[(address - 0xA000) + (0x2000 * curr_ram_bank)] = data;
      }
      else ram_banks[address - 0xA000] = data;
    }
  }
}

uint8_t Memory::mbc3_read(uint16_t address) const{
  if (address <= 0x3FFF) {
    return cart[address];
  }

  // reading from ROM bank
  else if (address >= 0x4000 && address <= 0x7FFF) {
    return cart[(address - 0x4000) + (0x4000 * curr_rom_bank)];
  }

  // reading from RAM bank
  else if (address >= 0xA000 && address <= 0xBFFF) {
    // printf("attempted ram read\n");
    if (ram_enabled) {
      return ram_banks[(0x2000 * curr_ram_bank) + (address - 0xA000)];
    }
    else return 0xFF;
  }

  // this statement should be unreachable in theory
  return 0xFF;
}

void Memory::mbc3_write(uint16_t address, uint8_t data) {
  if (address < 0x2000) {
    ram_enabled = (data & 0xF) == 0xA;
  } 
  else if (address < 0x4000) {
    if ((data & 0b01111111) == 0) curr_rom_bank = 1;
    else curr_rom_bank = data & 0b01111111;
  }
  else if (address < 0x6000){
  }
  else if (address < 0x8000) {
  }
  else {
    // printf("attempted ram write\n");
    if (ram_enabled) {
      ram_banks[(address - 0xA000) + (0x2000 * curr_ram_bank)] = data;
    }
  }
}

uint8_t Memory::mbc_read(unsigned short address) const{
  if (banking_type == MBC1 || banking_type == MBC1_RAM
      || banking_type == MBC1_RAM_BATTERY) {
    return mbc1_read(address);
  } 
  else if (banking_type == MBC3 || banking_type == MBC3_RAM
           || banking_type == MBC3_RAM_BATTERY) {
    return mbc3_read(address);
  }

  return 0xFF;
}

void Memory::mbc_write(unsigned short address, unsigned char data) {
  if (banking_type == MBC1 || banking_type == MBC1_RAM
      || banking_type == MBC1_RAM_BATTERY) {
    mbc1_write(address, data);
  }
  else if (banking_type == MBC3 || banking_type == MBC3_RAM
          || banking_type == MBC3_RAM_BATTERY) {
    mbc3_write(address, data);
  }
}

void Memory::write_byte(unsigned short address, unsigned char data) {
  // 0x0000-0x7FFF is read only
  if (address < 0x8000 || (address >= 0xA000 && address < 0xC000)) {
    mbc_write(address, data);
  }

  // VRAM
  else if (address >= 0x8000 && address <= 0x9FFF) {
    if (get_ppu_mode() != 3) {
      mem[address] = data;
    }
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

  // OAM
  else if (address >= 0xFE00 && address <= 0xFE9F) {
    if (get_ppu_mode() < 2) {
      mem[address] = data;
    }
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

  else if (address == LCD_STATUS) {
    // printf("set LCD STATUS to %d\n", data);
    mem[address] = (mem[address] & 0b10000111) | (data & 0b01111000);
  }

  else if(address == LCD_CONTROL) {
    bool prev_enabled = is_lcd_enabled();
    mem[address] = data;
    if (is_lcd_enabled() && !prev_enabled) {
      // if (cpu->state != BOOTING) printf("lcd enabled\n");
      check_lyc_ly();
    }
    // else if (!is_lcd_enabled() && prev_enabled) {
    //   if (cpu-> state != BOOTING) printf("lcd disabled\n");
    // }
  }

  else {
    mem[address] = data;
  }
}


unsigned char Memory::read_byte(unsigned short address) const {
  if (address < 0x100 && cpu->state == BOOTING) {
    return boot_rom[address];
  }

  if (address < 0x4000) {
    if (banking_type != NO_BANKING) {
      return mbc_read(address);
    }
    return cart[address];
  }

  // reading from ROM bank
  else if (address >= 0x4000 && address <= 0x7FFF) {
    if (banking_type != NO_BANKING) {
      return mbc_read(address);
    }
    return cart[(address - 0x4000) + (curr_rom_bank * 0x4000)];
  }

  // VRAM
  else if (address >= 0x8000 && address <= 0x9FFF) {
    if (get_ppu_mode() == 3) {
      return 0xFF;
    }
    return mem[address];
  }

  // reading from RAM bank
  else if (address >= 0xA000 && address <= 0xBFFF) {
    if (banking_type != NO_BANKING) {
      return mbc_read(address);
    }
    return ram_banks[(address - 0xA000) + (curr_ram_bank * 0x2000)];
  }
  
  // OAM
  else if (address >= 0xFE00 && address <= 0xFE9F) {
    if (get_ppu_mode() >= 2) {
      return 0xFF;
    }
    return mem[address];
  }

  else if (address >= DIV_REG && address <= TAC_REG) {
    return timer->timer_read(address);
  }

  else if (address == 0xFF00) { // JOYPAD REGISTER
    // printf("read %d from joypad\n", joypad->get_joypad_state());
    // mem[0xFF00] = joypad->get_joypad_state();
    return joypad->get_joypad_state();
  }

  else if (address == IF_REG) {
    return mem[address] | 0xE0;
  }

  else if (address == LCD_STATUS) {
    return mem[LCD_STATUS] | 0b10000000;
    // printf("STAT: %d\n", mem[LCD_STATUS]);
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

  if (!cpu->ime && bit == STAT_INTER) {
    return;
  }
  write_byte(IF_REG, read_byte(IF_REG) | (1 << bit));
}

void Memory::reset_scanline() {
  mem[LY] = 0;
  check_lyc_ly();
}

// void Memory::set_scanline(uint8_t ly) {
//   mem[LY] = ly;
// }

void Memory::inc_scanline() {
  mem[LY]++; 
  check_lyc_ly();
}

void Memory::check_lyc_ly() {
  if (is_lcd_enabled()) {
    if (mem[LY] == mem[LYC]) {
      if (((mem[LCD_STATUS] >> 2) & 1) == 1) {
        return;
      }
      mem[LCD_STATUS] = mem[LCD_STATUS] | (1 << 2);
      if ((mem[LCD_STATUS] >> 6) & 0x1) {
        request_interrupt(STAT_INTER);
      }
    }
    else {
      mem[LCD_STATUS] = mem[LCD_STATUS] & ~(1 << 2);
    }
  }
}

void Memory::dma_transfer(uint8_t data) {
  // printf("dma transfer %02X %d %d\n", data, mem[LY], mem[LCD_STATUS] & 0x3);
  uint16_t xfer_i = data << 8;
  for (int i = 0xFE00; i < 0xFEA0; i++) {
    // mem[i] = mem[xfer_i];
    mem[i] = read_byte(xfer_i);
    xfer_i++;
  }
}

bool Memory::is_lcd_enabled() const {
  return (mem[LCD_CONTROL] >> 7) & 1;
}

uint8_t Memory::get_ppu_mode() const {
  return mem[LCD_STATUS] & 0x3;
}

void Memory::set_ppu_mode(uint8_t mode) {
  mem[LCD_STATUS] = (mem[LCD_STATUS] & 0b11111100) | (mode & 0b00000011);
}
