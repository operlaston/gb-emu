#include "memory.hh"
#include "constants.hh"
#include "timer.hh"
#include "joypad.hh"
#include <cstdio>
#include <iostream>
#include <istream>

static unsigned char mem[0x10000];
static unsigned char cart[0x200000];
static unsigned char ram_banks[0x8000]; // a ram bank is 0x2000 in size and there are 4 max

Memory::Memory(char *rom_path){

  FILE *rom_fp = fopen(rom_path, "rb"); // open in read binary mode
  if (rom_fp == NULL) {
    std::cout << "Failed to load rom with filepath " << rom_path << std::endl;
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
      banking_type = NONE;
      break;
    case 0x1:
      banking_type = MBC1;
      printf("MBC1\n");
      break;
    case 0x2:
      banking_type = MBC1_RAM;
      printf("MBC1\n");
      break;
    case 0x3:
      banking_type = MBC1_RAM_BATTERY;
      printf("MBC1\n");
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
      printf("This MBC type is not supported. Only MBC1 and MBC3 are currently supported\n");
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
  curr_rom_bank = 1; // rom bank at 0x4000-0x7fff (default is 1)
  curr_ram_bank = 0;

  ram_enabled = false;
  mode_flag = false;

  // copy rom into memory
  // for (unsigned long i = 0; i < 0x8000; i++) {
  //   mem[i] = cart[i];
  // }

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
    // TODO: banking for roms >= 1MB

    return cart[address];
  }

  // reading from ROM bank
  else if (address >= 0x4000 && address <= 0x7FFF) {
    // printf("curr rom bank: %d\n", curr_rom_bank);
    uint32_t offset = 0x4000 * curr_rom_bank;
    return cart[(address - 0x4000) + offset];
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
  return 0;
}

void Memory::mbc1_write(unsigned short address, unsigned char data) {
  if (address < 0x2000) {
    ram_enabled = (data & 0xF) == 0xA;
  } 
  else if (address < 0x4000) {
    // uint8_t n = data == 0 ? 1 : data;
    // curr_rom_bank = (curr_rom_bank & 0b01100000) | (n & 0b00011111);
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
    // curr_ram_bank = data & 0x3;
    // if (mode_flag) {
    //   curr_ram_bank = data & 0x3;
    // }
    // else {
    //   curr_rom_bank = (curr_rom_bank & 0b00011111) | ((data & 3) << 5);
    // }
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

void Memory::handle_banking(unsigned short address, unsigned char data) {
  if (banking_type == MBC1 || banking_type == MBC1_RAM || banking_type == MBC1_RAM_BATTERY) {
    mbc1_write(address, data);
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

  else if (address == LCD_STATUS) {
    // printf("set LCD STATUS to %d\n", data);
    mem[address] = mem[address] | (data & 0b01111000);
  }

  else if(address == LCD_CONTROL) {
    bool prev_enabled = is_lcd_enabled();
    mem[address] = data;
    if (is_lcd_enabled() && !prev_enabled) {
      check_lyc_ly();
    }
  }

  else {
    mem[address] = data;
  }
}


unsigned char Memory::read_byte(unsigned short address) const {
  if (address < 0x4000) {
    if (banking_type == MBC1 || banking_type == MBC1_RAM || banking_type == MBC1_RAM_BATTERY) {
      return mbc1_read(address);
    }
    return cart[address];
  }

  // reading from ROM bank
  if (address >= 0x4000 && address <= 0x7FFF) {
    if (banking_type == MBC1 || banking_type == MBC1_RAM || banking_type == MBC1_RAM_BATTERY) {
      return mbc1_read(address);
    }
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

  else if (address == IF_REG) {
    return mem[address] | 0xE0;
  }

  if (address == LCD_STATUS) {
    return mem[LCD_STATUS] | 0b10000000;
    // printf("STAT: %d\n", mem[LCD_STATUS]);
  }

  // if (address == LCD_CONTROL) {
  //   if (!lcd_on) {
  //     printf("lcd isn't on. LCD: %d\n", mem[address]);
  //   }
  // }

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
  // if (!is_lcd_enabled()) {
  //   printf("LY = %d\tLYC = %d\n", mem[LY], mem[LYC]);
  // }
  if (is_lcd_enabled()) {
    if (mem[LY] == mem[LYC]) {
      mem[LCD_STATUS] = mem[LCD_STATUS] | (1 << 2);
      // printf("set ly==lyc to 1\n");
      // printf("LYC = LY\n");
      // printf("LCD_STATUS = %d\n", mem[LCD_STATUS]);
      if ((mem[LCD_STATUS] >> 6) & 0x1) {
        // printf("LYC = LY interrupt requested\n");
        request_interrupt(STAT_INTER);
      }
    }
    else {
      mem[LCD_STATUS] = mem[LCD_STATUS] & ~(1 << 2);
    }
  }
}

void Memory::dma_transfer(uint8_t data) {
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
