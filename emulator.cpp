#include "emulator.h"
#include <cstdint>
#include <pthread.h>
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
  if (fsize > sizeof(rom)) { 
    cout << "rom is too large" << endl;
    fclose(rom_fp);
    rom_fp = NULL;
    exit(1);
  }
  if (fread(rom, 1, fsize, rom_fp) != fsize) { 
    cout << "failed to read rom contents" << endl;
    fclose(rom_fp);
    rom_fp = NULL;
    exit(1);
  }

  fclose(rom_fp);
  rom_fp = NULL;

  if (rom[0x147] <= 3 && rom[0x147] >= 1) { 
    rom_banking_type = MBC1;
  }
  else if (rom[0x147] == 5 || rom[0x147] == 6) { 
    rom_banking_type = MBC2;
  }
  else {
    rom_banking_type = NONE;
  }

  num_rom_banks = 2 << rom[0x148];
  if (rom[0x148] > 6) {
    cout << "invalid byte at 0x148 for rom size" << endl;
    exit(1);
  }

  switch(rom[0x149]) {
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
    checksum = checksum - rom[address] - 1;
  }
  if (checksum != rom[0x14D]) {
    cout << "checksum did not pass. stopped during init" << endl;
    exit(1);
  }

  curr_rom_bank = 1; // rom bank at 0x4000-0x7fff (default is 1)
  memset(ram_banks, 0, sizeof(ram_banks));
  curr_ram_bank = 0;

  ram_enabled = false;

  // opcode table entry example
  opcode_table[0x2] = [this](){ ld_r16_a(REG_BC); };

  // Instruction instruction_table[256] = {
  //
  // };
  //
  // // instructions prefixed with cb
  // Instruction cb_table[256] = {
  //
  // };
}

Emulator::~Emulator() {

}

unsigned char Emulator::read_byte(unsigned short address) const {
  // reading from ROM bank
  if (address >= 0x4000 && address <= 0x7FFF) {
    unsigned short offset = address - 0x4000;
    return rom[offset + (curr_rom_bank * 0x4000)];
  }

  // reading from RAM bank
  else if (address >= 0xA000 && address <= 0xBFFF) {
    unsigned short offset = address - 0xA000;
    return ram_banks[offset + (curr_ram_bank * 0x2000)];
  }
  
  return mem[address];
}

// this function assumes little endian
unsigned short Emulator::read_word(unsigned short address) const {
  unsigned char lower_byte = read_byte(address);
  unsigned char upper_byte = read_byte(address + 1);
  return (upper_byte << 8) | lower_byte;
}

void Emulator::write_byte(unsigned short address, unsigned char data) {
  // 0x0000-0x7FFF is read only
  if (address < 0x8000) {
    handle_banking(address, data);
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

void Emulator::handle_banking(unsigned short address, unsigned char data) {
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

void Emulator::write_r8(REGISTER r8, unsigned char data) {
  unsigned char *reg = find_r8(r8);
  *reg = data;
}

void Emulator::write_r16(REGISTER r16, unsigned short data) {
  unsigned short *reg = find_r16(r16);
  *reg = data;
}

unsigned char Emulator::read_r8(REGISTER r8) {
  unsigned char *reg = find_r8(r8);
  return *reg;
}

unsigned short Emulator::read_r16(REGISTER r16) {
  unsigned short *reg = find_r16(r16);
  return *reg;
}

unsigned char Emulator::next8() {
  unsigned char data = read_byte(pc);
  pc++;
  return data;
}

unsigned short Emulator::next16() {
  unsigned short data = read_word(pc);
  pc += 2;
  return data;
}

void Emulator::set_flag(int flagbit, bool set) {
  // create the bit mask using bit shift
  unsigned char mask = 1 << flagbit;
  unsigned char *flag_reg = find_r8(REG_F);
  // if we want to disable the bit, flip the mask and use bitwise &
  if (!set) {
    *flag_reg = *flag_reg & ~mask;
  }
  // if we want to set the bit, bitwise |
  else {
    *flag_reg = *flag_reg | mask;
  }
}

bool Emulator::get_flag(int flagbit) { 
  unsigned char mask = 1 << flagbit;
  return read_r8(REG_F) & mask;
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

void Emulator::cycle() {
  unsigned char opcode = read_byte(pc);
  pc++;
  auto& opcode_function = opcode_table[opcode];
  if (opcode_function) {
    opcode_function();
  }
  else {
    cout << "unknown opcode detected. exiting now..." << endl;
    exit(1);
  }
}

// it is up to the caller to know whether the register
// they are looking for is a 1 byte register or a 2 byte register
unsigned char *Emulator::find_r8(REGISTER reg) {
  unsigned char *target_register;
  switch(reg) {
    case REG_A:
      target_register = &AF.first;
      break;
    case REG_B:
      target_register = &BC.first;
      break;
    case REG_C:
      target_register = &BC.second;
      break;
    case REG_D:
      target_register = &DE.first;
      break;
    case REG_E:
      target_register = &DE.second;
      break;
    case REG_F:
      target_register = &AF.second;
      break;
    case REG_H:
      target_register = &HL.first;
      break;
    case REG_L:
      target_register = &HL.second;
      break;
    default:
      return NULL;
  }
  return target_register; 
}

unsigned short *Emulator::find_r16(REGISTER reg) {
  unsigned short *target_register;
  switch(reg) {
    case REG_AF:
      target_register = &AF.reg;
      break;
    case REG_BC:
      target_register = &BC.reg;
      break;
    case REG_DE:
      target_register = &DE.reg;
      break;
    case REG_HL:
      target_register = &HL.reg;
      break;
    default:
      return NULL;
  }
  return target_register;
}

/*
 * load instructions
 */

void Emulator::ld_r8_r8(REGISTER reg1, REGISTER reg2) {
  // copy reg2 into reg1
  unsigned char *reg1_ptr = find_r8(reg1);
  unsigned char *reg2_ptr = find_r8(reg2);
  *reg1_ptr = *reg2_ptr;
}

void Emulator::ld_r8_n8(REGISTER r8) {
  // copy n8 into r8 
  unsigned short n8 = read_byte(pc);
  pc++;
  unsigned char *reg = find_r8(r8);
  *reg = n8;
}

void Emulator::ld_r16_n16(REGISTER r16) {
  // copy n16 into r16
  // assumes little endian
  unsigned short n16 = read_word(pc);
  pc += 2;
  unsigned short *reg = find_r16(r16);
  *reg = n16;
}

void Emulator::ld_hl_r8(REGISTER r8) {
  // copy the value in r8 into the byte pointed to by HL
  unsigned short byte_loc = HL.reg;
  write_byte(byte_loc, read_r8(r8));
}

void Emulator::ld_hl_n8() {
  // copy the value in n8 into the byte pointed to by HL
  unsigned char n8 = read_byte(pc);
  pc++;
  unsigned short byte_loc = HL.reg;
  write_byte(byte_loc, n8);
}

void Emulator::ld_r8_hl(REGISTER r8) {
  // copy the value pointed to by HL into r8
  unsigned short byte_loc = HL.reg;
  unsigned char byte_val = read_byte(byte_loc);
  unsigned char *reg = find_r8(r8);
  *reg = byte_val;
}

void Emulator::ld_r16_a(REGISTER r16) {
  // copy the value in register A into the byte pointed to by r16
  unsigned char reg_a = AF.first;
  unsigned short r16_loc = *find_r16(r16);
  write_byte(r16_loc, reg_a);
}

void Emulator::ld_n16_a() {
  // copy the value in register A into the byte at address n16
  unsigned short loc = read_word(pc);
  pc += 2;
  write_byte(loc, AF.first);
}

void Emulator::ldh_n8_a() {
  // copy the value in register A into the byte at address n8
  // provided the address is between 0xFF00 and 0xFFFF
  unsigned char loc = read_byte(pc);
  pc += 1;
  loc = 0xFF00 + loc;
  write_byte(loc, AF.first);
}

void Emulator::ldh_c_a() {
  // copy the value in register A into the byte at address 0xFF00 + C
  write_byte(0xFF00 + BC.second, AF.first);
}

void Emulator::ld_a_r16(REGISTER r16) {
  // copy the byte pointed to by r16 into register A  
  unsigned short *reg = find_r16(r16);
  unsigned char val = read_byte(*reg);
  write_r8(REG_A, val);
}

void Emulator::ld_a_n16() {
  unsigned short n16 = next16();
  write_r8(REG_A, read_byte(n16));
}

void Emulator::ldh_a_n8() {
  // load into register A the data from the address specified by 
  // n8 + 0xFF00
  unsigned short n8 = next8();
  write_r8(REG_A, read_byte(0xFF00 + n8));
}

void Emulator::ldh_a_c() {
  // copy the byte from address 0xFF00 + C into register A
  unsigned char val = 0xFF00 + read_r8(REG_C);
  write_r8(REG_A, val);
}

void Emulator::ld_hli_a() {
  // copy the value in register A into the byte pointed to by HL
  // and increment HL afterwards
  unsigned char val = read_r8(REG_A);
  unsigned short loc = HL.reg;
  write_byte(loc, val);
  HL.reg++;
}

void Emulator::ld_hld_a() {
  // copy A into byte pointed by HL and decrement HL
  unsigned char val = read_r8(REG_A);
  unsigned short loc = HL.reg;
  write_byte(loc, val);
  HL.reg--;
}

void Emulator::ld_a_hld() {
  // copy byte pointed by HL into A and decrement HL after
  unsigned char val = read_byte(HL.reg);
  write_r8(REG_A, val);
  HL.reg--;
}

void Emulator::ld_a_hli() {
  // copy byte pointed by HL into A and increment HL after
  write_r8(REG_A, read_byte(HL.reg));
  HL.reg--;
}


/*
 * 8-bit arithmetic instructions
 */

void Emulator::adc_a_helper(uint8_t val) {
  uint8_t carry_flag = get_flag(FLAG_C) ? 1 : 0;
  uint8_t prev = AF.first;
  AF.first = AF.first + val + carry_flag;
  uint16_t res = prev + val + carry_flag;

  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, ((prev & 0xF) + (val & 0xF) + (carry_flag & 0xF)) > 0xF);
  set_flag(FLAG_C, res > 0xFF);
}

void Emulator::adc_a_r8(REGISTER r8) {
  // add the value in r8 plus the carry flag to A
  adc_a_helper(read_r8(r8));
}

void Emulator::adc_a_hl() {
  adc_a_helper(read_byte(HL.reg));
}

void Emulator::adc_a_n8() {
  adc_a_helper(next8());
}

void Emulator::add_a_helper(uint8_t val) {
  // helper for the following add instructions
  uint8_t prev = AF.first;
  AF.first = AF.first + val;
  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, (prev & 0xF) + (val & 0xF) > 0xF);
  set_flag(FLAG_C, prev + val > 0xFF);
}

void Emulator::add_a_r8(REGISTER r8) {
  add_a_helper(read_r8(r8));
}

void Emulator::add_a_hl() {
  add_a_helper(read_byte(HL.reg));
}

void Emulator::add_a_n8() {
  add_a_helper(next8());
}

void Emulator::cp_a_helper(uint8_t val) {
  set_flag(FLAG_Z, AF.first == val);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (AF.first & 0xF) < (val & 0xF));
  set_flag(FLAG_C, AF.first < val);
}

void Emulator::cp_a_r8(REGISTER r8) {
  cp_a_helper(read_r8(r8));
}

void Emulator::cp_a_hl() {
  cp_a_helper(read_byte(HL.reg));
}

void Emulator::cp_a_n8() {
  cp_a_helper(next8());
}

void Emulator::dec_r8(REGISTER r8) {
  uint8_t *reg = find_r8(r8);
  uint8_t prev = *reg;
  *reg = *reg - 1;
  set_flag(FLAG_Z, *reg == 0);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (prev & 0xF) == 0);
}

void Emulator::dec_hl() {
  uint8_t prev = read_byte(HL.reg);
  write_byte(HL.reg, prev - 1);
  set_flag(FLAG_Z, read_byte(HL.reg) == 0);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (prev & 0xF) == 0);
}

void Emulator::inc_r8(REGISTER r8) {
  uint8_t *reg = find_r8(r8);
  uint8_t prev = *reg;
  *reg = *reg + 1;
  set_flag(FLAG_Z, *reg == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, (prev & 0xF) == 0xF);
}

void Emulator::inc_hl() {
  uint8_t prev = read_byte(HL.reg);
  write_byte(HL.reg, prev + 1);
  set_flag(FLAG_Z, read_byte(HL.reg) == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, (prev & 0xF) == 0xF);  
}

void Emulator::sbc_a_helper(uint8_t val) {
  uint8_t carry_flag = get_flag(FLAG_C) ? 1 : 0;
  uint8_t prev = AF.first;
  AF.first = AF.first - val - carry_flag;
  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (val & 0xF) + (carry_flag & 0xF) > (prev & 0xF));
  set_flag(FLAG_C, val + carry_flag > prev);
}

void Emulator::sbc_a_r8(REGISTER r8) {
  sbc_a_helper(read_r8(r8));
}

void Emulator::sbc_a_hl() {
  sbc_a_helper(read_byte(HL.reg));
}

void Emulator::sbc_a_n8() {
  sbc_a_helper(next8());
}

void Emulator::sub_a_helper(uint8_t val) {
  uint8_t prev = AF.first;
  AF.first -= val;
  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (prev & 0xF) < (val & 0xF));
  set_flag(FLAG_C, prev < val);
}

void Emulator::sub_a_r8(REGISTER r8) {
  sub_a_helper(read_r8(r8));
}

void Emulator::sub_a_hl() {
  sub_a_helper(read_byte(HL.reg));
}

void Emulator::sub_a_n8() {
  sub_a_helper(next8());
}


/*
 * 16-bit arithmetic instructions
 */

void Emulator::add_hl_r16(REGISTER r16) {
  uint16_t val = read_r16(r16);
  uint16_t prev = HL.reg;
  HL.reg += val;
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, (prev & 0xFFF) + (val & 0xFFF) > 0xFFF);
  set_flag(FLAG_C, prev + val > 0xFFFF);
}

void Emulator::dec_r16(REGISTER r16) {
  uint16_t *reg = find_r16(r16);
  *reg -= 1;
}

void Emulator::inc_r16(REGISTER r16) {
  uint16_t *reg = find_r16(r16);
  *reg += 1;
}


/*
 * bitwise logic instructions
 */

void Emulator::and_a_helper(uint8_t val) {
  AF.first &= val;
  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 1);
  set_flag(FLAG_C, 0);
}

void Emulator::and_a_r8(REGISTER r8) {
  and_a_helper(read_r8(r8));
}

void Emulator::and_a_hl() {
  and_a_helper(read_byte(HL.reg));
}

void Emulator::and_a_n8() {
  and_a_helper(next8());
}

void Emulator::cpl() {
  AF.first = ~AF.first;
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, 1);
}

void Emulator::or_a_helper(uint8_t val) {
  AF.first |= val;
  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
  set_flag(FLAG_C, 0);
}

void Emulator::or_a_r8(REGISTER r8) {
  or_a_helper(read_r8(r8));
}

void Emulator::or_a_hl() {
  or_a_helper(read_byte(HL.reg));
}

void Emulator::or_a_n8() {
  or_a_helper(next8());
}

void Emulator::xor_a_helper(uint8_t val) {
  AF.first ^= val;
  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
  set_flag(FLAG_C, 0);
}

void Emulator::xor_a_r8(REGISTER r8) {
  xor_a_helper(read_r8(r8));
}

void Emulator::xor_a_hl() {
  xor_a_helper(read_byte(HL.reg));
}

void Emulator::xor_a_n8() {
  xor_a_helper(next8());
}


/*
 * bit flag instructions
 */

void Emulator::bit_u3_r8(uint8_t bit, REGISTER r8) {
  uint8_t mask = 1 << bit;
  uint8_t reg = read_r8(r8);
  set_flag(FLAG_Z, (reg & mask) == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 1);
}

void Emulator::bit_u3_hl(uint8_t bit) {
  uint8_t mask = 1 << bit;
  uint8_t val = read_byte(HL.reg);
  set_flag(FLAG_Z, (val & mask) == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 1);
}

void Emulator::res_u3_r8(uint8_t bit, REGISTER r8) {
  uint8_t mask = 1 << bit;
  mask = ~mask;
  uint8_t *reg = find_r8(r8);
  *reg &= mask;
}

void Emulator::res_u3_hl(uint8_t bit) {
  uint8_t mask = 1 << bit;
  mask = ~mask;
  write_byte(HL.reg, read_byte(HL.reg) & mask);
}

void Emulator::set_u3_r8(uint8_t bit, REGISTER r8) {
  uint8_t mask = 1 << bit;
  uint8_t *reg = find_r8(r8);
  *reg |= mask;
}

void Emulator::set_u3_hl(uint8_t bit) {
  uint8_t mask = 1 << bit;
  write_byte(HL.reg, read_byte(HL.reg) | mask);
}


/*
 * bit shift instructions
 */

void Emulator::set_shift_flags(uint8_t val) {
  set_flag(FLAG_Z, val == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
}

void Emulator::rl_r8(REGISTER r8) {
  uint8_t reg = read_r8(r8);
  uint8_t carry_flag = get_flag(FLAG_C) ? 1 : 0;
  set_flag(FLAG_C, reg & 0x80); //most significant bit
  reg <<= 1;
  reg += carry_flag;
  write_r8(r8, reg);

  set_shift_flags(reg);
}

void Emulator::rl_hl() {
  uint8_t val = read_byte(HL.reg);
  uint8_t carry_flag = get_flag(FLAG_C) ? 1 : 0;
  set_flag(FLAG_C, val & 0x80); //most significant bit
  val <<= 1;
  val += carry_flag;
  write_byte(HL.reg, val);

  set_shift_flags(val);
}

void Emulator::rla() {
  uint8_t val = AF.first;
  uint8_t carry_flag = get_flag(FLAG_C) ? 1 : 0;
  set_flag(FLAG_C, val & 0x80); //most significant bit
  val <<= 1;
  val += carry_flag;
  AF.first = val;

  set_shift_flags(1); // RESET zero flag
}

void Emulator::rlc_r8(REGISTER r8) {
  uint8_t reg = read_r8(r8);
  uint8_t msb = reg & 0x80 ? 1 : 0;
  set_flag(FLAG_C, msb); //most significant bit
  reg <<= 1;
  reg += msb;
  write_r8(r8, reg);

  set_shift_flags(reg);
}

void Emulator::rlc_hl() {
  uint8_t val = read_byte(HL.reg);
  uint8_t msb = val & 0x80 ? 1 : 0;
  set_flag(FLAG_C, msb); //most significant bit
  val <<= 1;
  val += msb;
  write_byte(HL.reg, val);

  set_shift_flags(val);
}

void Emulator::rlca() {
  uint8_t val = AF.first;
  uint8_t msb = val & 0x80 ? 1 : 0;
  set_flag(FLAG_C, msb); //most significant bit
  val <<= 1;
  val += msb;
  AF.first = val;

  set_shift_flags(1); // RESET zero flag
}

void Emulator::rr_r8(REGISTER r8) {
  uint8_t reg = read_r8(r8);
  uint8_t carry_flag = get_flag(FLAG_C) ? 0x80 : 0;
  set_flag(FLAG_C, reg & 0x1); //least significant bit
  reg >>= 1;
  reg |= carry_flag;
  write_r8(r8, reg);

  set_shift_flags(reg);
}

void Emulator::rr_hl() {
  uint8_t val = read_byte(HL.reg);
  uint8_t carry_flag = get_flag(FLAG_C) ? 0x80 : 0;
  set_flag(FLAG_C, val & 0x1); //least significant bit
  val >>= 1;
  val |= carry_flag;
  write_byte(HL.reg, val);

  set_shift_flags(val);
}

void Emulator::rra() {
  uint8_t val = AF.first;
  uint8_t carry_flag = get_flag(FLAG_C) ? 0x80 : 0;
  set_flag(FLAG_C, val & 0x1); //least significant bit
  val >>= 1;
  val |= carry_flag;
  AF.first = val;
  set_shift_flags(1); // RESET zero flag
}

void Emulator::rrc_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  uint8_t lsb = val & 0x1 ? 0x80 : 0;
  set_flag(FLAG_C, lsb); //least significant bit
  val >>= 1;
  val |= lsb;
  write_r8(r8, val);
  set_shift_flags(val);
}

void Emulator::rrc_hl() {
  uint8_t val = read_byte(HL.reg);
  uint8_t lsb = val & 0x1 ? 0x80 : 0;
  set_flag(FLAG_C, lsb); //least significant bit
  val >>= 1;
  val |= lsb;
  write_byte(HL.reg, val);
  set_shift_flags(val);
}

void Emulator::rrca() {
  uint8_t val = AF.first;
  uint8_t lsb = val & 0x1 ? 0x80 : 0;
  set_flag(FLAG_C, lsb); //least significant bit
  val >>= 1;
  val |= lsb;
  AF.first = val;
  set_shift_flags(1); //RESET zero flag
}

void Emulator::sla_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  set_flag(FLAG_C, val & 0x80);
  val <<= 1;
  write_r8(r8, val);
  set_shift_flags(val);
}

void Emulator::sla_hl() {
  uint8_t val = read_byte(HL.reg);
  set_flag(FLAG_C, val & 0x80);
  val <<= 1;
  write_byte(HL.reg, val);
  set_shift_flags(val);
}

void Emulator::sra_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  set_flag(FLAG_C, val & 0x1);
  uint8_t mask = val & 0x80; // msb
  val >>= 1;
  val |= mask;
  write_r8(r8, val);
  set_shift_flags(val);
}

void Emulator::sra_hl() {
  uint8_t val = read_byte(HL.reg);
  set_flag(FLAG_C, val & 0x1);
  uint8_t mask = val & 0x80; // msb
  val >>= 1;
  val |= mask;
  write_byte(HL.reg, val);
  set_shift_flags(val);
}

void Emulator::srl_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  set_flag(FLAG_C, val & 0x1);
  val >>= 1;
  write_r8(r8, val);
  set_shift_flags(val);
}

void Emulator::srl_hl() {
  uint8_t val = read_byte(HL.reg);
  set_flag(FLAG_C, val & 0x1);
  val >>= 1;
  write_byte(HL.reg, val);
  set_shift_flags(val);
}


/*
 * stack manipulation instructions
 */

void Emulator::ld_sp_n16() {
  // copy n16 into sp
  sp = next16();
}

void Emulator::ld_n16_sp() {
  // copy sp into n16 and n16 + 1
  // little endian so write least significant byte first
  unsigned short n16 = next16();
  write_byte(n16, sp & 0xFF); 
  write_byte(n16 + 1, sp >> 8);
}

void Emulator::ld_hl_sp_e8() {
  // add e8 to sp and copy result into HL; don't update sp
  int8_t e8 = next8();
  HL.reg = e8 + sp;  
  set_flag(FLAG_Z, 0);
  set_flag(FLAG_N, 0);
  if ((e8 & 0xF) + (sp & 0xF) > 0xF) {
    set_flag(FLAG_H, 1);
  }
  else set_flag(FLAG_H, 0);
  // perform and unsigned addition to determine carry flags
  if ((e8 & 0xFF) + (sp & 0xFF) > 0xFF) {
    set_flag(FLAG_C, 1);
  }
  else set_flag(FLAG_C, 0);
}

void Emulator::ld_sp_hl() {
  // copy HL into sp
  sp = HL.reg;
}


/*
* special instructions
*/

void Emulator::nop() {
  // do nothing
  return;
}

