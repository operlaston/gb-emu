#include "cpu.hh"
#include "constants.hh"
#include <cstdint>
#include <cstring>
#include <pthread.h>
#include <cstdio>
#include <iostream>
using namespace std;

Cpu::Cpu(Memory& mem) : mmu(mem) {
  // initialize program state (put in higher level class later)
  state = RUNNING;

  // initialize program counter, stack pointer, registers
  pc = 0x100;
  sp = 0xFFFE;
  AF.reg = 0x01B0;
  BC.reg = 0x0013;
  DE.reg = 0x00D8;
  HL.reg = 0x014D;
  ime = 0;
  set_ime = false;
  is_prefix = false;
  instr_cycles = 0;

  // set screen
  // memset(screen, 0, sizeof(screen));

  init_opcode_table();
  init_prefix_table();

  //cout << "set up instruction tables and initialized memory" << endl;
  cout << "initalized cpu" << endl;
}

unsigned char *Cpu::find_r8(REGISTER reg) {
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

unsigned short *Cpu::find_r16(REGISTER reg) {
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

void Cpu::write_r8(REGISTER r8, unsigned char data) {
  unsigned char *reg = find_r8(r8);
  *reg = data;
}

void Cpu::write_r16(REGISTER r16, unsigned short data) {
  unsigned short *reg = find_r16(r16);
  *reg = data;
}

unsigned char Cpu::read_r8(REGISTER r8) {
  unsigned char *reg = find_r8(r8);
  return *reg;
}

unsigned short Cpu::read_r16(REGISTER r16) {
  unsigned short *reg = find_r16(r16);
  return *reg;
}

unsigned char Cpu::next8() {
  unsigned char data = mmu.read_byte(pc);
  pc++;
  return data;
}

unsigned short Cpu::next16() {
  unsigned short data = mmu.read_word(pc);
  pc += 2;
  return data;
}

void Cpu::set_flag(int flagbit, bool set) {
  // create the bit mask using bit shift
  unsigned char mask = 1 << flagbit;
  // if we want to disable the bit, flip the mask and use bitwise &
  if (!set) {
    AF.second &= ~mask;
  }
  // if we want to set the bit, bitwise |
  else {
    AF.second |= mask;
  }
}

bool Cpu::get_flag(int flagbit) { 
  unsigned char mask = 1 << flagbit;
  return read_r8(REG_F) & mask;
}

void Cpu::service_interrupt() {
  // IE (interrupt enable): 0xFFFF
  // IF (interrupt flag/requested): 0xFF0F

  uint8_t if_reg = mmu.read_byte(IF_REG);
  uint8_t ief = if_reg & mmu.read_byte(IE_REG); // enabled and requested
  if (ief && state == HALTED) {
    state = RUNNING;
  }

  if (!ime) {
    return;
  }

  uint16_t interrupt_address = 0;
  uint8_t interrupt_type = 0;
  for(int i = 0; i < 5; i++) {
    uint8_t bitmask = 1;
    bitmask <<= i;
    if (ief & bitmask) {
      interrupt_type = i;
      switch(i) {
        case VBLANK_INTER:
          interrupt_address = VBLANK_HANDLER;
          break;
        case STAT_INTER:
          interrupt_address = STAT_HANDLER;
          break;
        case TIMER_INTER:
          interrupt_address = TIMER_HANDLER;
          break;
        case SERIAL_INTER:
          interrupt_address = SERIAL_HANDLER;
          break;
        case JOYPAD_INTER:
          interrupt_address = JOYPAD_HANDLER;
          break;
      }
      break;
    }
  }

  if (interrupt_address == 0) return;
  mmu.write_byte(--sp, pc >> 8);
  mmu.write_byte(--sp, pc & 0xFF);
  
  pc = interrupt_address;
  uint8_t mask = 1 << interrupt_type;
  mmu.write_byte(IF_REG, if_reg & ~mask);
}

// void Cpu::update_timers(uint8_t cycles) {
//   mmu.update_div(cycles);
//   // while (div_cycles >= 0xFF) {
//   //   mmu.inc_div(); // increments DIV register after every 256 t-cycles
//   //   div_cycles -= 0xFF;
//   // }
//
//   uint8_t tac = mmu.read_byte(TAC_REG);
//   if (!(tac & 0x4)) { // clock is disabled
//     return;
//   }
//   tac &= 0x3;
//   uint8_t tima = mmu.read_byte(TIMA_REG);
//   uint32_t max_tima_cycles = 1024; // tac == 0 (every 1024 t-cycles or 256 m-cycles)
//   if (tac == 1) {
//     // 262144 hz or increment every 16 t-cycles or 4 m-cycles
//     max_tima_cycles = 16;
//   }
//   else if (tac == 2) {
//     // 65536 hz or increment every 64 t-cycles or 16 m-cycles
//     max_tima_cycles = 64;
//   }
//   else {
//     // 16384 hz of increment every 256 t-cycles or 64 m-cycles
//     max_tima_cycles = 256;
//   }
//   tima_cycles += cycles;
//   while (tima_cycles >= max_tima_cycles) {
//     tima_cycles -= max_tima_cycles;
//     if (tima == 0xFF) {
//       mmu.write_byte(TIMA_REG, mmu.read_byte(TMA_REG));
//       mmu.request_interrupt(TIMER_INTER);
//     }
//     else {
//       mmu.write_byte(TIMA_REG, tima + 1);
//     }
//   }
// }

uint8_t Cpu::fetch_and_execute() {
  instr_cycles = 0;
  unsigned char opcode = mmu.read_byte(pc);
  pc++;
  auto& opcode_function = opcode_table[opcode];
  // handle 0xCB (prefix instruction); execute prefixed instruction immediately
  if (opcode == 0xCB) {
    opcode = mmu.read_byte(pc);
    pc++;
    opcode_function = prefix_table[opcode];
  }

  if (opcode_function) {
    opcode_function();
    AF.second &= 0xF0;
    if (is_last_instr_ei) {
      is_last_instr_ei = false;
    }
    else if (set_ime) {
      ime = 1;
      set_ime = false;
    }
    // increment number of cycles/ticks based on the instruction
  }
  else {
    cout << "unknown opcode detected. exiting now..." << endl;
    exit(1);
  }
  return (instr_cycles << 2); // convert to T-cycles
}

// void Cpu::update() {
//   // max cycles per frame (60 frames per second)
//   const int CYCLES_PER_FRAME = CYCLES_PER_SECOND / 60;
//   int cycles_this_update = 0;
//   while (cycles_this_update < CYCLES_PER_FRAME) {
//     // perform a cycle
//     uint8_t cycles = 0;
//     if (state == RUNNING) cycles = fetch_and_execute();
//     else if (state == HALTED) cycles = 1;
//     cycles_this_update += cycles;
//     // update timers
//     update_timers(cycles);
//     // update graphics
//     // do interrupts
//     service_interrupt();
//   }
//   // render the screen
// }


/*
 * load instructions
 */

void Cpu::ld_r8_r8(REGISTER reg1, REGISTER reg2) {
  // copy reg2 into reg1
  write_r8(reg1, read_r8(reg2));
  instr_cycles = 1;
}

void Cpu::ld_r8_n8(REGISTER r8) {
  // copy n8 into r8 
  uint8_t n8 = next8();
  write_r8(r8, n8);
  instr_cycles = 2;
}

void Cpu::ld_r16_n16(REGISTER r16) {
  // copy n16 into r16
  uint16_t n16 = next16();
  write_r16(r16, n16);
  instr_cycles = 3;
}

void Cpu::ld_hl_r8(REGISTER r8) {
  // copy the value in r8 into the byte pointed to by HL
  unsigned short loc = HL.reg;
  mmu.write_byte(loc, read_r8(r8));
  instr_cycles = 2;
}

void Cpu::ld_hl_n8() {
  // copy the value in n8 into the byte pointed to by HL
  unsigned char n8 = next8();
  mmu.write_byte(HL.reg, n8);
  instr_cycles = 3;
}

void Cpu::ld_r8_hl(REGISTER r8) {
  // copy the value pointed to by HL into r8
  write_r8(r8, mmu.read_byte(HL.reg));
  instr_cycles = 2;
}

void Cpu::ld_r16_a(REGISTER r16) {
  // copy the value in register A into the byte pointed to by r16
  mmu.write_byte(read_r16(r16), AF.first);
  instr_cycles = 2;
}

void Cpu::ld_n16_a() {
  // copy the value in register A into the byte at address n16
  unsigned short loc = mmu.read_word(pc);
  pc += 2;
  mmu.write_byte(loc, AF.first);
  instr_cycles = 4;
}

void Cpu::ldh_n8_a() {
  // copy the value in register A into the byte at address n8
  // provided the address is between 0xFF00 and 0xFFFF
  unsigned char loc = mmu.read_byte(pc);
  pc += 1;
  mmu.write_byte(0xFF00 + loc, AF.first);
  instr_cycles = 3;
}

void Cpu::ldh_c_a() {
  // copy the value in register A into the byte at address 0xFF00 + C
  mmu.write_byte(0xFF00 + BC.second, AF.first);
  instr_cycles = 2;
}

void Cpu::ld_a_r16(REGISTER r16) {
  // copy the byte pointed to by r16 into register A  
  unsigned short *reg = find_r16(r16);
  unsigned char val = mmu.read_byte(*reg);
  write_r8(REG_A, val);
  instr_cycles = 2;
}

void Cpu::ld_a_n16() {
  unsigned short n16 = next16();
  write_r8(REG_A, mmu.read_byte(n16));
  instr_cycles = 4;
}

void Cpu::ldh_a_n8() {
  // load into register A the data from the address specified by 
  // n8 + 0xFF00
  unsigned char n8 = next8();
  write_r8(REG_A, mmu.read_byte(0xFF00 + n8));
  instr_cycles = 3;
}

void Cpu::ldh_a_c() {
  // copy the byte from address 0xFF00 + C into register A
  unsigned short val = 0xFF00 + read_r8(REG_C);
  write_r8(REG_A, mmu.read_byte(val));
  instr_cycles = 2;
}

void Cpu::ld_hli_a() {
  // copy the value in register A into the byte pointed to by HL
  // and increment HL afterwards
  unsigned char val = read_r8(REG_A);
  unsigned short loc = HL.reg;
  mmu.write_byte(loc, val);
  HL.reg++;
  instr_cycles = 2;
}

void Cpu::ld_hld_a() {
  // copy A into byte pointed by HL and decrement HL
  unsigned char val = read_r8(REG_A);
  unsigned short loc = HL.reg;
  mmu.write_byte(loc, val);
  HL.reg--;
  instr_cycles = 2;
}

void Cpu::ld_a_hld() {
  // copy byte pointed by HL into A and decrement HL after
  unsigned char val = mmu.read_byte(HL.reg);
  write_r8(REG_A, val);
  HL.reg--;
  instr_cycles = 2;
}

void Cpu::ld_a_hli() {
  // copy byte pointed by HL into A and increment HL after
  write_r8(REG_A, mmu.read_byte(HL.reg));
  HL.reg++;
  instr_cycles = 2;
}


/*
 * 8-bit arithmetic instructions
 */

void Cpu::adc_a_helper(uint8_t val) {
  uint8_t carry_flag = get_flag(FLAG_C) ? 1 : 0;
  uint8_t prev = AF.first;
  AF.first = AF.first + val + carry_flag;
  uint16_t res = prev + val + carry_flag;

  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, ((prev & 0xF) + (val & 0xF) + (carry_flag & 0xF)) > 0xF);
  set_flag(FLAG_C, res > 0xFF);
}

void Cpu::adc_a_r8(REGISTER r8) {
  // add the value in r8 plus the carry flag to A
  adc_a_helper(read_r8(r8));
  instr_cycles = 1;
}

void Cpu::adc_a_hl() {
  adc_a_helper(mmu.read_byte(HL.reg));
  instr_cycles = 2;
}

void Cpu::adc_a_n8() {
  adc_a_helper(next8());
  instr_cycles = 2;
}

void Cpu::add_a_helper(uint8_t val) {
  // helper for the following add instructions
  uint8_t prev = AF.first;
  AF.first = AF.first + val;
  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, (prev & 0xF) + (val & 0xF) > 0xF);
  set_flag(FLAG_C, prev + val > 0xFF);
}

void Cpu::add_a_r8(REGISTER r8) {
  add_a_helper(read_r8(r8));
  instr_cycles = 1;
}

void Cpu::add_a_hl() {
  add_a_helper(mmu.read_byte(HL.reg));
  instr_cycles = 2;
}

void Cpu::add_a_n8() {
  add_a_helper(next8());
  instr_cycles = 2;
}

void Cpu::cp_a_helper(uint8_t val) {
  set_flag(FLAG_Z, AF.first == val);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (AF.first & 0xF) < (val & 0xF));
  set_flag(FLAG_C, AF.first < val);
}

void Cpu::cp_a_r8(REGISTER r8) {
  cp_a_helper(read_r8(r8));
  instr_cycles = 1;
}

void Cpu::cp_a_hl() {
  cp_a_helper(mmu.read_byte(HL.reg));
  instr_cycles = 2;
}

void Cpu::cp_a_n8() {
  cp_a_helper(next8());
  instr_cycles = 2;
}

void Cpu::dec_r8(REGISTER r8) {
  uint8_t *reg = find_r8(r8);
  uint8_t prev = *reg;
  *reg = *reg - 1;
  set_flag(FLAG_Z, *reg == 0);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (prev & 0xF) == 0);
  instr_cycles = 1;
}

void Cpu::dec_hl() {
  uint8_t prev = mmu.read_byte(HL.reg);
  mmu.write_byte(HL.reg, prev - 1);
  set_flag(FLAG_Z, mmu.read_byte(HL.reg) == 0);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (prev & 0xF) == 0);
  instr_cycles = 3;
}

void Cpu::inc_r8(REGISTER r8) {
  uint8_t *reg = find_r8(r8);
  uint8_t prev = *reg;
  *reg = *reg + 1;
  set_flag(FLAG_Z, *reg == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, (prev & 0xF) == 0xF);
  instr_cycles = 1;
}

void Cpu::inc_hl() {
  uint8_t prev = mmu.read_byte(HL.reg);
  mmu.write_byte(HL.reg, prev + 1);
  set_flag(FLAG_Z, mmu.read_byte(HL.reg) == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, (prev & 0xF) == 0xF);  
  instr_cycles = 3;
}

void Cpu::sbc_a_helper(uint8_t val) {
  uint8_t carry_flag = get_flag(FLAG_C) ? 1 : 0;
  uint8_t prev = AF.first;
  AF.first = AF.first - val - carry_flag;
  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (val & 0xF) + (carry_flag & 0xF) > (prev & 0xF));
  set_flag(FLAG_C, val + carry_flag > prev);
}

void Cpu::sbc_a_r8(REGISTER r8) {
  sbc_a_helper(read_r8(r8));
  instr_cycles = 1;
}

void Cpu::sbc_a_hl() {
  sbc_a_helper(mmu.read_byte(HL.reg));
  instr_cycles = 2;
}

void Cpu::sbc_a_n8() {
  sbc_a_helper(next8());
  instr_cycles = 2;
}

void Cpu::sub_a_helper(uint8_t val) {
  uint8_t prev = AF.first;
  AF.first -= val;
  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (prev & 0xF) < (val & 0xF));
  set_flag(FLAG_C, prev < val);
}

void Cpu::sub_a_r8(REGISTER r8) {
  sub_a_helper(read_r8(r8));
  instr_cycles = 1;
}

void Cpu::sub_a_hl() {
  sub_a_helper(mmu.read_byte(HL.reg));
  instr_cycles = 2;
}

void Cpu::sub_a_n8() {
  sub_a_helper(next8());
  instr_cycles = 2;
}


/*
 * 16-bit arithmetic instructions
 */

void Cpu::add_hl_r16(REGISTER r16) {
  uint16_t val = read_r16(r16);
  uint16_t prev = HL.reg;
  HL.reg += val;
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, (prev & 0xFFF) + (val & 0xFFF) > 0xFFF);
  set_flag(FLAG_C, prev + val > 0xFFFF);
  instr_cycles = 2;
}

void Cpu::dec_r16(REGISTER r16) {
  uint16_t *reg = find_r16(r16);
  *reg -= 1;
  instr_cycles = 2;
}

void Cpu::inc_r16(REGISTER r16) {
  uint16_t *reg = find_r16(r16);
  *reg += 1;
  instr_cycles = 2;
}


/*
 * bitwise logic instructions
 */

void Cpu::and_a_helper(uint8_t val) {
  AF.first &= val;
  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 1);
  set_flag(FLAG_C, 0);
}

void Cpu::and_a_r8(REGISTER r8) {
  and_a_helper(read_r8(r8));
  instr_cycles = 1;
}

void Cpu::and_a_hl() {
  and_a_helper(mmu.read_byte(HL.reg));
  instr_cycles = 2;
}

void Cpu::and_a_n8() {
  and_a_helper(next8());
  instr_cycles = 2;
}

void Cpu::cpl() {
  AF.first = ~AF.first;
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, 1);
  instr_cycles = 1;
}

void Cpu::or_a_helper(uint8_t val) {
  AF.first |= val;
  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
  set_flag(FLAG_C, 0);
}

void Cpu::or_a_r8(REGISTER r8) {
  or_a_helper(read_r8(r8));
  instr_cycles = 1;
}

void Cpu::or_a_hl() {
  or_a_helper(mmu.read_byte(HL.reg));
  instr_cycles = 2;
}

void Cpu::or_a_n8() {
  or_a_helper(next8());
  instr_cycles = 2;
}

void Cpu::xor_a_helper(uint8_t val) {
  AF.first ^= val;
  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
  set_flag(FLAG_C, 0);
}

void Cpu::xor_a_r8(REGISTER r8) {
  xor_a_helper(read_r8(r8));
  instr_cycles = 1;
}

void Cpu::xor_a_hl() {
  xor_a_helper(mmu.read_byte(HL.reg));
  instr_cycles = 2;
}

void Cpu::xor_a_n8() {
  xor_a_helper(next8());
  instr_cycles = 2;
}


/*
 * bit flag instructions
 */

void Cpu::bit_u3_r8(uint8_t bit, REGISTER r8) {
  uint8_t mask = 1 << bit;
  uint8_t reg = read_r8(r8);
  set_flag(FLAG_Z, (reg & mask) == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 1);
  instr_cycles = 2;
}

void Cpu::bit_u3_hl(uint8_t bit) {
  uint8_t mask = 1 << bit;
  uint8_t val = mmu.read_byte(HL.reg);
  set_flag(FLAG_Z, (val & mask) == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 1);
  instr_cycles = 3;
}

void Cpu::res_u3_r8(uint8_t bit, REGISTER r8) {
  uint8_t mask = 1 << bit;
  mask = ~mask;
  uint8_t *reg = find_r8(r8);
  *reg &= mask;
  instr_cycles = 2;
}

void Cpu::res_u3_hl(uint8_t bit) {
  uint8_t mask = 1 << bit;
  mask = ~mask;
  mmu.write_byte(HL.reg, mmu.read_byte(HL.reg) & mask);
  instr_cycles = 4;
}

void Cpu::set_u3_r8(uint8_t bit, REGISTER r8) {
  uint8_t mask = 1 << bit;
  uint8_t *reg = find_r8(r8);
  *reg |= mask;
  instr_cycles = 2;
}

void Cpu::set_u3_hl(uint8_t bit) {
  uint8_t mask = 1 << bit;
  mmu.write_byte(HL.reg, mmu.read_byte(HL.reg) | mask);
  instr_cycles = 4;
}


/*
 * bit shift instructions
 */

void Cpu::set_shift_flags(uint8_t val) {
  set_flag(FLAG_Z, val == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
}

void Cpu::rl_r8(REGISTER r8) {
  uint8_t reg = read_r8(r8);
  uint8_t carry_flag = get_flag(FLAG_C) ? 1 : 0;
  set_flag(FLAG_C, reg & 0x80); //most significant bit
  reg <<= 1;
  reg += carry_flag;
  write_r8(r8, reg);

  set_shift_flags(reg);
  instr_cycles = 2;
}

void Cpu::rl_hl() {
  uint8_t val = mmu.read_byte(HL.reg);
  uint8_t carry_flag = get_flag(FLAG_C) ? 1 : 0;
  set_flag(FLAG_C, val & 0x80); //most significant bit
  val <<= 1;
  val += carry_flag;
  mmu.write_byte(HL.reg, val);

  set_shift_flags(val);
  instr_cycles = 4;
}

void Cpu::rla() {
  uint8_t val = AF.first;
  uint8_t carry_flag = get_flag(FLAG_C) ? 1 : 0;
  set_flag(FLAG_C, val & 0x80); //most significant bit
  val <<= 1;
  val += carry_flag;
  AF.first = val;

  set_shift_flags(1); // RESET zero flag
  instr_cycles = 1;
}

void Cpu::rlc_r8(REGISTER r8) {
  uint8_t reg = read_r8(r8);
  uint8_t msb = reg & 0x80 ? 1 : 0;
  set_flag(FLAG_C, msb); //most significant bit
  reg <<= 1;
  reg += msb;
  write_r8(r8, reg);

  set_shift_flags(reg);
  instr_cycles = 2;
}

void Cpu::rlc_hl() {
  uint8_t val = mmu.read_byte(HL.reg);
  uint8_t msb = val & 0x80 ? 1 : 0;
  set_flag(FLAG_C, msb); //most significant bit
  val <<= 1;
  val += msb;
  mmu.write_byte(HL.reg, val);

  set_shift_flags(val);
  instr_cycles = 4;
}

void Cpu::rlca() {
  uint8_t val = AF.first;
  uint8_t msb = val & 0x80 ? 1 : 0;
  set_flag(FLAG_C, msb); //most significant bit
  val <<= 1;
  val += msb;
  AF.first = val;

  set_shift_flags(1); // RESET zero flag
  instr_cycles = 1;
}

void Cpu::rr_r8(REGISTER r8) {
  uint8_t reg = read_r8(r8);
  uint8_t carry_flag = get_flag(FLAG_C) ? 0x80 : 0;
  set_flag(FLAG_C, reg & 0x1); //least significant bit
  reg >>= 1;
  reg |= carry_flag;
  write_r8(r8, reg);

  set_shift_flags(reg);
  instr_cycles = 2;
}

void Cpu::rr_hl() {
  uint8_t val = mmu.read_byte(HL.reg);
  uint8_t carry_flag = get_flag(FLAG_C) ? 0x80 : 0;
  set_flag(FLAG_C, val & 0x1); //least significant bit
  val >>= 1;
  val |= carry_flag;
  mmu.write_byte(HL.reg, val);

  set_shift_flags(val);
  instr_cycles = 4;
}

void Cpu::rra() {
  uint8_t val = AF.first;
  uint8_t carry_flag = get_flag(FLAG_C) ? 0x80 : 0;
  set_flag(FLAG_C, val & 0x1); //least significant bit
  val >>= 1;
  val |= carry_flag;
  AF.first = val;
  set_shift_flags(1); // RESET zero flag
  instr_cycles = 1;
}

void Cpu::rrc_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  uint8_t lsb = val & 0x1 ? 0x80 : 0;
  set_flag(FLAG_C, lsb); //least significant bit
  val >>= 1;
  val |= lsb;
  write_r8(r8, val);
  set_shift_flags(val);
  instr_cycles = 2;
}

void Cpu::rrc_hl() {
  uint8_t val = mmu.read_byte(HL.reg);
  uint8_t lsb = val & 0x1 ? 0x80 : 0;
  set_flag(FLAG_C, lsb); //least significant bit
  val >>= 1;
  val |= lsb;
  mmu.write_byte(HL.reg, val);
  set_shift_flags(val);
  instr_cycles = 4;
}

void Cpu::rrca() {
  uint8_t val = AF.first;
  uint8_t lsb = val & 0x1 ? 0x80 : 0;
  set_flag(FLAG_C, lsb); //least significant bit
  val >>= 1;
  val |= lsb;
  AF.first = val;
  set_shift_flags(1); //RESET zero flag
  instr_cycles = 1;
}

void Cpu::sla_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  set_flag(FLAG_C, val & 0x80);
  val <<= 1;
  write_r8(r8, val);
  set_shift_flags(val);
  instr_cycles = 2;
}

void Cpu::sla_hl() {
  uint8_t val = mmu.read_byte(HL.reg);
  set_flag(FLAG_C, val & 0x80);
  val <<= 1;
  mmu.write_byte(HL.reg, val);
  set_shift_flags(val);
  instr_cycles = 4;
}

void Cpu::sra_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  set_flag(FLAG_C, val & 0x1);
  uint8_t mask = val & 0x80; // msb
  val >>= 1;
  val |= mask;
  write_r8(r8, val);
  set_shift_flags(val);
  instr_cycles = 2;
}

void Cpu::sra_hl() {
  uint8_t val = mmu.read_byte(HL.reg);
  set_flag(FLAG_C, val & 0x1);
  uint8_t mask = val & 0x80; // msb
  val >>= 1;
  val |= mask;
  mmu.write_byte(HL.reg, val);
  set_shift_flags(val);
  instr_cycles = 4;
}

void Cpu::srl_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  set_flag(FLAG_C, val & 0x1);
  val >>= 1;
  write_r8(r8, val);
  set_shift_flags(val);
  instr_cycles = 2;
}

void Cpu::srl_hl() {
  uint8_t val = mmu.read_byte(HL.reg);
  set_flag(FLAG_C, val & 0x1);
  val >>= 1;
  mmu.write_byte(HL.reg, val);
  set_shift_flags(val);
  instr_cycles = 4;
}

void Cpu::swap_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  uint8_t lower_four = val & 0xF;
  lower_four <<= 4;
  val >>= 4;
  val |= lower_four;
  write_r8(r8, val);
  set_flag(FLAG_Z, val == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
  set_flag(FLAG_C, 0);
  instr_cycles = 2;
}

void Cpu::swap_hl() {
  uint8_t val = mmu.read_byte(HL.reg);
  uint8_t lower_four = val & 0xF;
  lower_four <<= 4;
  val >>= 4;
  val |= lower_four;
  mmu.write_byte(HL.reg, val);
  set_flag(FLAG_Z, val == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
  set_flag(FLAG_C, 0);
  instr_cycles = 4;
}


/*
 * jumps and subroutine instructions
 */

void Cpu::call_n16() {
  uint16_t n16 = next16();
  // next16() increments the program counter so it should
  // be on the next instruction

  // push most significant byte first due to little endian
  // and stack growing downwards
  mmu.write_byte(--sp, pc >> 8);
  mmu.write_byte(--sp, pc & 0xFF);
  
  pc = n16;
  instr_cycles = 6;
}

void Cpu::call_cc_n16(int flag, bool condition) {
  if (get_flag(flag) == condition) {
    call_n16();
    // call_n16() sets the number of cycles
  }
  else {
    instr_cycles = 3;
    pc += 2;
  }
}

void Cpu::jp_hl() {
  pc = HL.reg;
  instr_cycles = 1; 
}

void Cpu::jp_n16() {
  uint16_t address = next16();
  pc = address;
  instr_cycles = 4;
}

void Cpu::jp_cc_n16(int flag, bool condition) {
  if (get_flag(flag) == condition) {
    jp_n16();
    // jp_n16() sets the number of cycles
  }
  else {
    pc += 2;
    instr_cycles = 3;
  }
}

void Cpu::jr_e8() {
  int8_t e8 = (int8_t)next8();
  pc += e8;
  instr_cycles = 3;
}

void Cpu::jr_cc_e8(int flag, bool condition) {
  if( get_flag(flag) == condition ) {
    jr_e8();
    // jr_e8() sets the number of cycles
  }
  else {
    pc++;
    instr_cycles = 2;
  }
}

void Cpu::ret() {
  pc = mmu.read_word(sp);
  sp += 2;
  instr_cycles = 4;
}

void Cpu::ret_cc(int flag, bool condition) {
  if (get_flag(flag) == condition) {
    ret();
    instr_cycles = 5;
  }
  else {
    instr_cycles = 2;  
  }
}

void Cpu::reti() {
  ret();
  ime = 1;
  instr_cycles = 4;
}

void Cpu::rst_vec(uint8_t n) {
  uint16_t addr = n | 0x0000; // extend to 2 bytes
  // pc is already at the next instruction
  mmu.write_byte(--sp, pc >> 8); // most significant byte
  mmu.write_byte(--sp, pc & 0xFF);
  pc = addr;
  instr_cycles = 4;
}

/*
 * carry flag instructions
 */

void Cpu::ccf() {
  set_flag(FLAG_C, !get_flag(FLAG_C));
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
  instr_cycles = 1;
}

void Cpu::scf() {
  set_flag(FLAG_C, 1);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
  instr_cycles = 1;
}


/*
 * stack manipulation instructions
 */

void Cpu::add_hl_sp() {
  uint16_t prev = HL.reg;
  HL.reg += sp;
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, (prev & 0xFFF) + (sp & 0xFFF) > 0xFFF);
  set_flag(FLAG_C, prev + sp > 0xFFFF);
  instr_cycles = 2;
}

void Cpu::add_sp_e8() {
  int8_t e8 = (int8_t)next8();
  uint8_t prev = sp & 0xFF;
  sp += e8;
  uint8_t u8 = (uint8_t)e8;
  
  set_flag(FLAG_Z, 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, (prev & 0xF) + (u8 & 0xF) > 0xF);
  set_flag(FLAG_C, prev + u8 > 0xFF);
  instr_cycles = 4;
}

void Cpu::dec_sp() {
  sp--;
  instr_cycles = 2;
}

void Cpu::inc_sp() {
  sp++;
  instr_cycles = 2;
}

void Cpu::ld_sp_n16() {
  // copy n16 into sp 
  sp = next16();
  instr_cycles = 3;
}

void Cpu::ld_n16_sp() {
  // copy sp into n16 and n16 + 1
  // little endian so write least significant byte first
  unsigned short n16 = next16();
  mmu.write_byte(n16, sp & 0xFF); 
  mmu.write_byte(n16 + 1, sp >> 8);
  instr_cycles = 5;
}

void Cpu::ld_hl_sp_e8() {
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
  instr_cycles = 3;
}

void Cpu::ld_sp_hl() {
  // copy HL into sp
  sp = HL.reg;
  instr_cycles = 2;
}

void Cpu::pop_af() {
  AF.second = mmu.read_byte(sp++);
  AF.first = mmu.read_byte(sp++);
  instr_cycles = 3;
}

void Cpu::pop_r16(REGISTER r16) {
  uint8_t low = mmu.read_byte(sp++);
  uint8_t high = mmu.read_byte(sp++);
  uint16_t val = (high << 8) | low;
  write_r16(r16, val);
  instr_cycles = 3;
}

void Cpu::push_af() {
  mmu.write_byte(--sp, AF.first);
  mmu.write_byte(--sp, AF.second);
  instr_cycles = 4;
}

void Cpu::push_r16(REGISTER r16) {
  uint16_t val = read_r16(r16);
  uint8_t high = val >> 8;
  uint8_t low = val & 0x00FF;

  mmu.write_byte(--sp, high);
  mmu.write_byte(--sp, low);
  instr_cycles = 4;
}


/*
 * interrupt related instructions
 */

void Cpu::di() {
  ime = 0;
  instr_cycles = 1;
}

void Cpu::ei() {
  set_ime = true;
  is_last_instr_ei = true;
  instr_cycles = 1;
}

void Cpu::halt() {
  bool interrupts_pending = mmu.read_byte(IE_REG) & mmu.read_byte(IF_REG);
  if (ime) {
    service_interrupt();
  }
  else {
    if (interrupts_pending) {

    }
    else {
      state = HALTED;
    }
  }
}


/*
* special instructions
*/

void Cpu::daa() {
  uint8_t adjustment = 0;
  if (get_flag(FLAG_N)) {
    if (get_flag(FLAG_H)) adjustment += 0x6;
    if (get_flag(FLAG_C)) adjustment += 0x60;
    AF.first -= adjustment;
  }
  else {
    if (get_flag(FLAG_H) || (AF.first & 0xF) > 0x9) adjustment += 0x6;
    if (get_flag(FLAG_C) || AF.first > 0x99) {
      adjustment += 0x60;
      set_flag(FLAG_C, 1);
    }
    AF.first += adjustment;
  }

  set_flag(FLAG_Z, AF.first == 0);
  set_flag(FLAG_H, 0);
  instr_cycles = 1;
}

void Cpu::nop() {
  // do nothing
  instr_cycles = 1;
}

void Cpu::stop() {
  //TODO
}
