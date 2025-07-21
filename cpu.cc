#include "cpu.hh"
#include "constants.hh"
#include <cstdint>
#include <cstring>
#include <pthread.h>
#include <cstdio>
using namespace std;

Cpu::Cpu(Memory& mem) : mmu(mem) {
  // initialize program state (put in higher level class later)
  state = BOOTING;
  // state = RUNNING; // skip boot

  // initialize program counter, stack pointer, registers
  // pc = 0x100; // skip boot
  pc = 0x0;
  sp = 0xFFFE;
  AF.reg = 0x01B0;
  BC.reg = 0x0013;
  DE.reg = 0x00D8;
  HL.reg = 0x014D;
  ime = 0;
  set_ime = false;
  is_prefix = false;
  halt_bug = false;
  instr_cycles = 0;
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

bool Cpu::service_interrupt() {
  // IE (interrupt enable): 0xFFFF
  // IF (interrupt flag/requested): 0xFF0F

  uint8_t if_reg = mmu.read_byte(IF_REG);
  uint8_t ief = if_reg & mmu.read_byte(IE_REG); // enabled and requested
  if (ief && state == HALTED) {
    state = RUNNING;
  }

  if (!ime) {
    return false;
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

  if (interrupt_address == 0) return false;
  mmu.write_byte(--sp, pc >> 8);
  mmu.write_byte(--sp, pc & 0xFF);
  
  pc = interrupt_address;
  uint8_t mask = 1 << interrupt_type;
  mmu.write_byte(IF_REG, if_reg & ~mask);
  ime = 0;
  return true;
}

uint8_t Cpu::fetch_and_execute() {
  static const void *opcode_table[] = {
    &&instr_00, &&instr_01, &&instr_02, &&instr_03, &&instr_04, &&instr_05, &&instr_06, &&instr_07,
    &&instr_08, &&instr_09, &&instr_0A, &&instr_0B, &&instr_0C, &&instr_0D, &&instr_0E, &&instr_0F,
    &&instr_10, &&instr_11, &&instr_12, &&instr_13, &&instr_14, &&instr_15, &&instr_16, &&instr_17,
    &&instr_18, &&instr_19, &&instr_1A, &&instr_1B, &&instr_1C, &&instr_1D, &&instr_1E, &&instr_1F,
    &&instr_20, &&instr_21, &&instr_22, &&instr_23, &&instr_24, &&instr_25, &&instr_26, &&instr_27,
    &&instr_28, &&instr_29, &&instr_2A, &&instr_2B, &&instr_2C, &&instr_2D, &&instr_2E, &&instr_2F,
    &&instr_30, &&instr_31, &&instr_32, &&instr_33, &&instr_34, &&instr_35, &&instr_36, &&instr_37,
    &&instr_38, &&instr_39, &&instr_3A, &&instr_3B, &&instr_3C, &&instr_3D, &&instr_3E, &&instr_3F,
    &&instr_40, &&instr_41, &&instr_42, &&instr_43, &&instr_44, &&instr_45, &&instr_46, &&instr_47,
    &&instr_48, &&instr_49, &&instr_4A, &&instr_4B, &&instr_4C, &&instr_4D, &&instr_4E, &&instr_4F,
    &&instr_50, &&instr_51, &&instr_52, &&instr_53, &&instr_54, &&instr_55, &&instr_56, &&instr_57,
    &&instr_58, &&instr_59, &&instr_5A, &&instr_5B, &&instr_5C, &&instr_5D, &&instr_5E, &&instr_5F,
    &&instr_60, &&instr_61, &&instr_62, &&instr_63, &&instr_64, &&instr_65, &&instr_66, &&instr_67,
    &&instr_68, &&instr_69, &&instr_6A, &&instr_6B, &&instr_6C, &&instr_6D, &&instr_6E, &&instr_6F,
    &&instr_70, &&instr_71, &&instr_72, &&instr_73, &&instr_74, &&instr_75, &&instr_76, &&instr_77,
    &&instr_78, &&instr_79, &&instr_7A, &&instr_7B, &&instr_7C, &&instr_7D, &&instr_7E, &&instr_7F,
    &&instr_80, &&instr_81, &&instr_82, &&instr_83, &&instr_84, &&instr_85, &&instr_86, &&instr_87,
    &&instr_88, &&instr_89, &&instr_8A, &&instr_8B, &&instr_8C, &&instr_8D, &&instr_8E, &&instr_8F,
    &&instr_90, &&instr_91, &&instr_92, &&instr_93, &&instr_94, &&instr_95, &&instr_96, &&instr_97,
    &&instr_98, &&instr_99, &&instr_9A, &&instr_9B, &&instr_9C, &&instr_9D, &&instr_9E, &&instr_9F,
    &&instr_A0, &&instr_A1, &&instr_A2, &&instr_A3, &&instr_A4, &&instr_A5, &&instr_A6, &&instr_A7,
    &&instr_A8, &&instr_A9, &&instr_AA, &&instr_AB, &&instr_AC, &&instr_AD, &&instr_AE, &&instr_AF,
    &&instr_B0, &&instr_B1, &&instr_B2, &&instr_B3, &&instr_B4, &&instr_B5, &&instr_B6, &&instr_B7,
    &&instr_B8, &&instr_B9, &&instr_BA, &&instr_BB, &&instr_BC, &&instr_BD, &&instr_BE, &&instr_BF,
    &&instr_C0, &&instr_C1, &&instr_C2, &&instr_C3, &&instr_C4, &&instr_C5, &&instr_C6, &&instr_C7,
    &&instr_C8, &&instr_C9, &&instr_CA, &&instr_unknown, &&instr_CC, &&instr_CD, &&instr_CE, &&instr_CF,
    &&instr_D0, &&instr_D1, &&instr_D2, &&instr_unknown, &&instr_D4, &&instr_D5, &&instr_D6, &&instr_D7,
    &&instr_D8, &&instr_D9, &&instr_DA, &&instr_unknown, &&instr_DC, &&instr_unknown, &&instr_DE, &&instr_DF,
    &&instr_E0, &&instr_E1, &&instr_E2, &&instr_unknown, &&instr_unknown, &&instr_E5, &&instr_E6, &&instr_E7,
    &&instr_E8, &&instr_E9, &&instr_EA, &&instr_unknown, &&instr_unknown, &&instr_unknown, &&instr_EE, &&instr_EF,
    &&instr_F0, &&instr_F1, &&instr_F2, &&instr_F3, &&instr_unknown, &&instr_F5, &&instr_F6, &&instr_F7,
    &&instr_F8, &&instr_F9, &&instr_FA, &&instr_FB, &&instr_unknown, &&instr_unknown, &&instr_FE, &&instr_FF
  };

  static const void *prefix_table[256] = {
    &&prefix_00, &&prefix_01, &&prefix_02, &&prefix_03, &&prefix_04, &&prefix_05, &&prefix_06, &&prefix_07,
    &&prefix_08, &&prefix_09, &&prefix_0A, &&prefix_0B, &&prefix_0C, &&prefix_0D, &&prefix_0E, &&prefix_0F,
    &&prefix_10, &&prefix_11, &&prefix_12, &&prefix_13, &&prefix_14, &&prefix_15, &&prefix_16, &&prefix_17,
    &&prefix_18, &&prefix_19, &&prefix_1A, &&prefix_1B, &&prefix_1C, &&prefix_1D, &&prefix_1E, &&prefix_1F,
    &&prefix_20, &&prefix_21, &&prefix_22, &&prefix_23, &&prefix_24, &&prefix_25, &&prefix_26, &&prefix_27,
    &&prefix_28, &&prefix_29, &&prefix_2A, &&prefix_2B, &&prefix_2C, &&prefix_2D, &&prefix_2E, &&prefix_2F,
    &&prefix_30, &&prefix_31, &&prefix_32, &&prefix_33, &&prefix_34, &&prefix_35, &&prefix_36, &&prefix_37,
    &&prefix_38, &&prefix_39, &&prefix_3A, &&prefix_3B, &&prefix_3C, &&prefix_3D, &&prefix_3E, &&prefix_3F,
    &&prefix_40, &&prefix_41, &&prefix_42, &&prefix_43, &&prefix_44, &&prefix_45, &&prefix_46, &&prefix_47,
    &&prefix_48, &&prefix_49, &&prefix_4A, &&prefix_4B, &&prefix_4C, &&prefix_4D, &&prefix_4E, &&prefix_4F,
    &&prefix_50, &&prefix_51, &&prefix_52, &&prefix_53, &&prefix_54, &&prefix_55, &&prefix_56, &&prefix_57,
    &&prefix_58, &&prefix_59, &&prefix_5A, &&prefix_5B, &&prefix_5C, &&prefix_5D, &&prefix_5E, &&prefix_5F,
    &&prefix_60, &&prefix_61, &&prefix_62, &&prefix_63, &&prefix_64, &&prefix_65, &&prefix_66, &&prefix_67,
    &&prefix_68, &&prefix_69, &&prefix_6A, &&prefix_6B, &&prefix_6C, &&prefix_6D, &&prefix_6E, &&prefix_6F,
    &&prefix_70, &&prefix_71, &&prefix_72, &&prefix_73, &&prefix_74, &&prefix_75, &&prefix_76, &&prefix_77,
    &&prefix_78, &&prefix_79, &&prefix_7A, &&prefix_7B, &&prefix_7C, &&prefix_7D, &&prefix_7E, &&prefix_7F,
    &&prefix_80, &&prefix_81, &&prefix_82, &&prefix_83, &&prefix_84, &&prefix_85, &&prefix_86, &&prefix_87,
    &&prefix_88, &&prefix_89, &&prefix_8A, &&prefix_8B, &&prefix_8C, &&prefix_8D, &&prefix_8E, &&prefix_8F,
    &&prefix_90, &&prefix_91, &&prefix_92, &&prefix_93, &&prefix_94, &&prefix_95, &&prefix_96, &&prefix_97,
    &&prefix_98, &&prefix_99, &&prefix_9A, &&prefix_9B, &&prefix_9C, &&prefix_9D, &&prefix_9E, &&prefix_9F,
    &&prefix_A0, &&prefix_A1, &&prefix_A2, &&prefix_A3, &&prefix_A4, &&prefix_A5, &&prefix_A6, &&prefix_A7,
    &&prefix_A8, &&prefix_A9, &&prefix_AA, &&prefix_AB, &&prefix_AC, &&prefix_AD, &&prefix_AE, &&prefix_AF,
    &&prefix_B0, &&prefix_B1, &&prefix_B2, &&prefix_B3, &&prefix_B4, &&prefix_B5, &&prefix_B6, &&prefix_B7,
    &&prefix_B8, &&prefix_B9, &&prefix_BA, &&prefix_BB, &&prefix_BC, &&prefix_BD, &&prefix_BE, &&prefix_BF,
    &&prefix_C0, &&prefix_C1, &&prefix_C2, &&prefix_C3, &&prefix_C4, &&prefix_C5, &&prefix_C6, &&prefix_C7,
    &&prefix_C8, &&prefix_C9, &&prefix_CA, &&prefix_CB, &&prefix_CC, &&prefix_CD, &&prefix_CE, &&prefix_CF,
    &&prefix_D0, &&prefix_D1, &&prefix_D2, &&prefix_D3, &&prefix_D4, &&prefix_D5, &&prefix_D6, &&prefix_D7,
    &&prefix_D8, &&prefix_D9, &&prefix_DA, &&prefix_DB, &&prefix_DC, &&prefix_DD, &&prefix_DE, &&prefix_DF,
    &&prefix_E0, &&prefix_E1, &&prefix_E2, &&prefix_E3, &&prefix_E4, &&prefix_E5, &&prefix_E6, &&prefix_E7,
    &&prefix_E8, &&prefix_E9, &&prefix_EA, &&prefix_EB, &&prefix_EC, &&prefix_ED, &&prefix_EE, &&prefix_EF,
    &&prefix_F0, &&prefix_F1, &&prefix_F2, &&prefix_F3, &&prefix_F4, &&prefix_F5, &&prefix_F6, &&prefix_F7,
    &&prefix_F8, &&prefix_F9, &&prefix_FA, &&prefix_FB, &&prefix_FC, &&prefix_FD, &&prefix_FE, &&prefix_FF 
  };
  
  instr_cycles = 0;
  if (state == BOOTING && pc == 0x100) state = RUNNING;
  unsigned char opcode = mmu.read_byte(pc);
  // print_registers();
  pc++;
  if (halt_bug) {
    pc--;
    halt_bug = false;
  }

  // handle 0xCB (prefix instruction); execute prefixed instruction immediately
  if (opcode == 0xCB) {
    opcode = mmu.read_byte(pc);
    pc++;
    // printf("prefix %02X\n", opcode);
    goto *prefix_table[opcode];
  }
  else {
    // printf("instr %02X\n", opcode);
    goto *opcode_table[opcode];
  }

instr_unknown:
  printf("unknown opcode detected. exiting now...\n");
  exit(1);

// miscellaneous instrs
instr_00:
  nop();
  goto after;
instr_10:
  stop();
  goto after;
instr_27:
  daa();
  goto after;

// interrupt related instrs
instr_76:
  halt();
  goto after;
instr_F3:
  di();
  goto after;
instr_FB:
  ei();
  goto after;

// stack manipulation instrs
instr_31:
  ld_sp_n16();
  goto after;
instr_33:
  inc_sp();
  goto after;
instr_08:
  ld_n16_sp();
  goto after;
instr_39:
  add_hl_sp();
  goto after;
instr_3B:
  dec_sp();
  goto after;
instr_C1:
  pop_r16(REG_BC);
  goto after;
instr_D1:
  pop_r16(REG_DE);
  goto after;
instr_E1:
  pop_r16(REG_HL);
  goto after;
instr_F1:
  pop_r16(REG_AF);
  goto after;
instr_C5:
  push_r16(REG_BC);
  goto after;
instr_D5:
  push_r16(REG_DE);
  goto after;
instr_E5:
  push_r16(REG_HL);
  goto after;
instr_F5:
  push_r16(REG_AF);
  goto after;
instr_E8:
  add_sp_e8();
  goto after;
instr_F8:
  ld_hl_sp_e8();
  goto after;
instr_F9:
  ld_sp_hl();
  goto after;

// carry flag instrs
instr_37:
  scf();
  goto after;
instr_3F:
  ccf();
  goto after;

// jumps and subroutine instrs
instr_20:
  jr_cc_e8(FLAG_Z, false);
  goto after;
instr_30:
  jr_cc_e8(FLAG_C, false);
  goto after;
instr_18:
  jr_e8();
  goto after;
instr_28:
  jr_cc_e8(FLAG_Z, true);
  goto after;
instr_38:
  jr_cc_e8(FLAG_C, true);
  goto after;
instr_C0:
  ret_cc(FLAG_Z, false);
  goto after;
instr_D0:
  ret_cc(FLAG_C, false);
  goto after;
instr_C2:
  jp_cc_n16(FLAG_Z, false);
  goto after;
instr_D2:
  jp_cc_n16(FLAG_C, false);
  goto after;
instr_C3:
  jp_n16();
  goto after;
instr_C4:
  call_cc_n16(FLAG_Z, false);
  goto after;
instr_D4:
  call_cc_n16(FLAG_C, false);
  goto after;
instr_C7:
  rst_vec(0x00);
  goto after;
instr_D7:
  rst_vec(0x10);
  goto after;
instr_E7:
  rst_vec(0x20);
  goto after;
instr_F7:
  rst_vec(0x30);
  goto after;
instr_C8:
  ret_cc(FLAG_Z, true);
  goto after;
instr_D8:
  ret_cc(FLAG_C, true);
  goto after;
instr_C9:
  ret();
  goto after;
instr_D9:
  reti();
  goto after;
instr_E9:
  jp_hl();
  goto after;
instr_CA:
  jp_cc_n16(FLAG_Z, true);
  goto after;
instr_DA:
  jp_cc_n16(FLAG_C, true);
  goto after;
instr_CC:
  call_cc_n16(FLAG_Z, true);
  goto after;
instr_DC:
  call_cc_n16(FLAG_C, true);
  goto after;
instr_CD:
  call_n16();
  goto after;
instr_CF:
  rst_vec(0x08);
  goto after;
instr_DF:
  rst_vec(0x18);
  goto after;
instr_EF:
  rst_vec(0x28);
  goto after;
instr_FF:
  rst_vec(0x38);
  goto after;

// bit shift instrs
instr_07:
  rlca();
  goto after;
instr_17:
  rla();
  goto after;
instr_0F:
  rrca();
  goto after;
instr_1F:
  rra();
  goto after;

// bitwise logic instrs
instr_2F:
  cpl();
  goto after;

// 16 bit arithmetic instrs
instr_03:
  inc_r16(REG_BC);
  goto after;
instr_13:
  inc_r16(REG_DE);
  goto after;
instr_23:
  inc_r16(REG_HL);
  goto after;
instr_09:
  add_hl_r16(REG_BC);
  goto after;
instr_19:
  add_hl_r16(REG_DE);
  goto after;
instr_29:
  add_hl_r16(REG_HL);
  goto after;
instr_0B:
  dec_r16(REG_BC);
  goto after;
instr_1B:
  dec_r16(REG_DE);
  goto after;
instr_2B:
  dec_r16(REG_HL);
  goto after;

// 8 bit arithmetic instrs
instr_04:
  inc_r8(REG_B);
  goto after;
instr_14:
  inc_r8(REG_D);
  goto after;
instr_24:
  inc_r8(REG_H);
  goto after;
instr_34:
  inc_hl();
  goto after;
instr_05:
  dec_r8(REG_B);
  goto after;
instr_15:
  dec_r8(REG_D);
  goto after;
instr_25:
  dec_r8(REG_H);
  goto after;
instr_35:
  dec_hl();
  goto after;
instr_0C:
  inc_r8(REG_C);
  goto after;
instr_1C:
  inc_r8(REG_E);
  goto after;
instr_2C:
  inc_r8(REG_L);
  goto after;
instr_3C:
  inc_r8(REG_A);
  goto after;
instr_0D:
  dec_r8(REG_C);
  goto after;
instr_1D:
  dec_r8(REG_E);
  goto after;
instr_2D:
  dec_r8(REG_L);
  goto after;
instr_3D:
  dec_r8(REG_A);
  goto after;
instr_80:
  add_a_r8(REG_B);
  goto after;
instr_81:
  add_a_r8(REG_C);
  goto after;
instr_82:
  add_a_r8(REG_D);
  goto after;
instr_83:
  add_a_r8(REG_E);
  goto after;
instr_84:
  add_a_r8(REG_H);
  goto after;
instr_85:
  add_a_r8(REG_L);
  goto after;
instr_86:
  add_a_hl();
  goto after;
instr_87:
  add_a_r8(REG_A);
  goto after;
instr_88:
  adc_a_r8(REG_B);
  goto after;
instr_89:
  adc_a_r8(REG_C);
  goto after;
instr_8A:
  adc_a_r8(REG_D);
  goto after;
instr_8B:
  adc_a_r8(REG_E);
  goto after;
instr_8C:
  adc_a_r8(REG_H);
  goto after;
instr_8D:
  adc_a_r8(REG_L);
  goto after;
instr_8E:
  adc_a_hl();
  goto after;
instr_8F:
  adc_a_r8(REG_A);
  goto after;
instr_90:
  sub_a_r8(REG_B);
  goto after;
instr_91:
  sub_a_r8(REG_C);
  goto after;
instr_92:
  sub_a_r8(REG_D);
  goto after;
instr_93:
  sub_a_r8(REG_E);
  goto after;
instr_94:
  sub_a_r8(REG_H);
  goto after;
instr_95:
  sub_a_r8(REG_L);
  goto after;
instr_96:
  sub_a_hl();
  goto after;
instr_97:
  sub_a_r8(REG_A);
  goto after;
instr_98:
  sbc_a_r8(REG_B);
  goto after;
instr_99:
  sbc_a_r8(REG_C);
  goto after;
instr_9A:
  sbc_a_r8(REG_D);
  goto after;
instr_9B:
  sbc_a_r8(REG_E);
  goto after;
instr_9C:
  sbc_a_r8(REG_H);
  goto after;
instr_9D:
  sbc_a_r8(REG_L);
  goto after;
instr_9E:
  sbc_a_hl();
  goto after;
instr_9F:
  sbc_a_r8(REG_A);
  goto after;
instr_A0:
  and_a_r8(REG_B);
  goto after;
instr_A1:
  and_a_r8(REG_C);
  goto after;
instr_A2:
  and_a_r8(REG_D);
  goto after;
instr_A3:
  and_a_r8(REG_E);
  goto after;
instr_A4:
  and_a_r8(REG_H);
  goto after;
instr_A5:
  and_a_r8(REG_L);
  goto after;
instr_A6:
  and_a_hl();
  goto after;
instr_A7:
  and_a_r8(REG_A);
  goto after;
instr_A8:
  xor_a_r8(REG_B);
  goto after;
instr_A9:
  xor_a_r8(REG_C);
  goto after;
instr_AA:
  xor_a_r8(REG_D);
  goto after;
instr_AB:
  xor_a_r8(REG_E);
  goto after;
instr_AC:
  xor_a_r8(REG_H);
  goto after;
instr_AD:
  xor_a_r8(REG_L);
  goto after;
instr_AE:
  xor_a_hl();
  goto after;
instr_AF:
  xor_a_r8(REG_A);
  goto after;
instr_B0:
  or_a_r8(REG_B);
  goto after;
instr_B1:
  or_a_r8(REG_C);
  goto after;
instr_B2:
  or_a_r8(REG_D);
  goto after;
instr_B3:
  or_a_r8(REG_E);
  goto after;
instr_B4:
  or_a_r8(REG_H);
  goto after;
instr_B5:
  or_a_r8(REG_L);
  goto after;
instr_B6:
  or_a_hl();
  goto after;
instr_B7:
  or_a_r8(REG_A);
  goto after;
instr_B8:
  cp_a_r8(REG_B);
  goto after;
instr_B9:
  cp_a_r8(REG_C);
  goto after;
instr_BA:
  cp_a_r8(REG_D);
  goto after;
instr_BB:
  cp_a_r8(REG_E);
  goto after;
instr_BC:
  cp_a_r8(REG_H);
  goto after;
instr_BD:
  cp_a_r8(REG_L);
  goto after;
instr_BE:
  cp_a_hl();
  goto after;
instr_BF:
  cp_a_r8(REG_A);
  goto after;
instr_C6:
  add_a_n8();
  goto after;
instr_D6:
  sub_a_n8();
  goto after;
instr_E6:
  and_a_n8();
  goto after;
instr_F6:
  or_a_n8();
  goto after;
instr_CE:
  adc_a_n8();
  goto after;
instr_DE:
  sbc_a_n8();
  goto after;
instr_EE:
  xor_a_n8();
  goto after;
instr_FE:
  cp_a_n8();
  goto after;

// load instrs
instr_01:
  ld_r16_n16(REG_BC);
  goto after;
instr_11:
  ld_r16_n16(REG_DE);
  goto after;
instr_21:
  ld_r16_n16(REG_HL);
  goto after;
instr_02:
  ld_r16_a(REG_BC);
  goto after;
instr_12:
  ld_r16_a(REG_DE);
  goto after;
instr_22:
  ld_hli_a();
  goto after;
instr_32:
  ld_hld_a();
  goto after;
instr_06:
  ld_r8_n8(REG_B);
  goto after;
instr_16:
  ld_r8_n8(REG_D);
  goto after;
instr_26:
  ld_r8_n8(REG_H);
  goto after;
instr_36:
  ld_hl_n8();
  goto after;
instr_46:
  ld_r8_hl(REG_B);
  goto after;
instr_56:
  ld_r8_hl(REG_D);
  goto after;
instr_66:
  ld_r8_hl(REG_H);
  goto after;
instr_0A:
  ld_a_r16(REG_BC);
  goto after;
instr_1A:
  ld_a_r16(REG_DE);
  goto after;
instr_2A:
  ld_a_hli();
  goto after;
instr_3A:
  ld_a_hld();
  goto after;
instr_0E:
  ld_r8_n8(REG_C);
  goto after;
instr_1E:
  ld_r8_n8(REG_E);
  goto after;
instr_2E:
  ld_r8_n8(REG_L);
  goto after;
instr_3E:
  ld_r8_n8(REG_A);
  goto after;
instr_40:
  ld_r8_r8(REG_B, REG_B);
  goto after;
instr_50:
  ld_r8_r8(REG_D, REG_B);
  goto after;
instr_60:
  ld_r8_r8(REG_H, REG_B);
  goto after;
instr_70:
  ld_hl_r8(REG_B);
  goto after;
instr_41:
  ld_r8_r8(REG_B, REG_C);
  goto after;
instr_51:
  ld_r8_r8(REG_D, REG_C);
  goto after;
instr_61:
  ld_r8_r8(REG_H, REG_C);
  goto after;
instr_71:
  ld_hl_r8(REG_C);
  goto after;
instr_42:
  ld_r8_r8(REG_B, REG_D);
  goto after;
instr_52:
  ld_r8_r8(REG_D, REG_D);
  goto after;
instr_62:
  ld_r8_r8(REG_H, REG_D);
  goto after;
instr_72:
  ld_hl_r8(REG_D);
  goto after;
instr_43:
  ld_r8_r8(REG_B, REG_E);
  goto after;
instr_53:
  ld_r8_r8(REG_D, REG_E);
  goto after;
instr_63:
  ld_r8_r8(REG_H, REG_E);
  goto after;
instr_73:
  ld_hl_r8(REG_E);
  goto after;
instr_44:
  ld_r8_r8(REG_B, REG_H);
  goto after;
instr_54:
  ld_r8_r8(REG_D, REG_H);
  goto after;
instr_64:
  ld_r8_r8(REG_H, REG_H);
  goto after;
instr_74:
  ld_hl_r8(REG_H);
  goto after;
instr_45:
  ld_r8_r8(REG_B, REG_L);
  goto after;
instr_55:
  ld_r8_r8(REG_D, REG_L);
  goto after;
instr_65:
  ld_r8_r8(REG_H, REG_L);
  goto after;
instr_75:
  ld_hl_r8(REG_L);
  goto after;
instr_47:
  ld_r8_r8(REG_B, REG_A);
  goto after;
instr_57:
  ld_r8_r8(REG_D, REG_A);
  goto after;
instr_67:
  ld_r8_r8(REG_H, REG_A);
  goto after;
instr_77:
  ld_hl_r8(REG_A);
  goto after;
instr_48:
  ld_r8_r8(REG_C, REG_B);
  goto after;
instr_58:
  ld_r8_r8(REG_E, REG_B);
  goto after;
instr_68:
  ld_r8_r8(REG_L, REG_B);
  goto after;
instr_78:
  ld_r8_r8(REG_A, REG_B);
  goto after;
instr_49:
  ld_r8_r8(REG_C, REG_C);
  goto after;
instr_59:
  ld_r8_r8(REG_E, REG_C);
  goto after;
instr_69:
  ld_r8_r8(REG_L, REG_C);
  goto after;
instr_79:
  ld_r8_r8(REG_A, REG_C);
  goto after;
instr_4A:
  ld_r8_r8(REG_C, REG_D);
  goto after;
instr_5A:
  ld_r8_r8(REG_E, REG_D);
  goto after;
instr_6A:
  ld_r8_r8(REG_L, REG_D);
  goto after;
instr_7A:
  ld_r8_r8(REG_A, REG_D);
  goto after;
instr_4B:
  ld_r8_r8(REG_C, REG_E);
  goto after;
instr_5B:
  ld_r8_r8(REG_E, REG_E);
  goto after;
instr_6B:
  ld_r8_r8(REG_L, REG_E);
  goto after;
instr_7B:
  ld_r8_r8(REG_A, REG_E);
  goto after;
instr_4C:
  ld_r8_r8(REG_C, REG_H);
  goto after;
instr_5C:
  ld_r8_r8(REG_E, REG_H);
  goto after;
instr_6C:
  ld_r8_r8(REG_L, REG_H);
  goto after;
instr_7C:
  ld_r8_r8(REG_A, REG_H);
  goto after;
instr_4D:
  ld_r8_r8(REG_C, REG_L);
  goto after;
instr_5D:
  ld_r8_r8(REG_E, REG_L);
  goto after;
instr_6D:
  ld_r8_r8(REG_L, REG_L);
  goto after;
instr_7D:
  ld_r8_r8(REG_A, REG_L);
  goto after;
instr_4E:
  ld_r8_hl(REG_C);
  goto after;
instr_5E:
  ld_r8_hl(REG_E);
  goto after;
instr_6E:
  ld_r8_hl(REG_L);
  goto after;
instr_7E:
  ld_r8_hl(REG_A);
  goto after;
instr_4F:
  ld_r8_r8(REG_C, REG_A);
  goto after;
instr_5F:
  ld_r8_r8(REG_E, REG_A);
  goto after;
instr_6F:
  ld_r8_r8(REG_L, REG_A);
  goto after;
instr_7F:
  ld_r8_r8(REG_A, REG_A);
  goto after;
instr_E0:
  ldh_n8_a();
  goto after;
instr_F0:
  ldh_a_n8();
  goto after;
instr_E2:
  ldh_c_a();
  goto after;
instr_F2:
  ldh_a_c();
  goto after;
instr_EA:
  ld_n16_a();
  goto after;
instr_FA:
  ld_a_n16();
  goto after;

// prefixed instrs
prefix_00:
  rlc_r8(REG_B);
  goto after;
prefix_01:
  rlc_r8(REG_C);
  goto after;
prefix_02:
  rlc_r8(REG_D);
  goto after;
prefix_03:
  rlc_r8(REG_E);
  goto after;
prefix_04:
  rlc_r8(REG_H);
  goto after;
prefix_05:
  rlc_r8(REG_L);
  goto after;
prefix_06:
  rlc_hl();
  goto after;
prefix_07:
  rlc_r8(REG_A);
  goto after;
prefix_08:
  rrc_r8(REG_B);
  goto after;
prefix_09:
  rrc_r8(REG_C);
  goto after;
prefix_0A:
  rrc_r8(REG_D);
  goto after;
prefix_0B:
  rrc_r8(REG_E);
  goto after;
prefix_0C:
  rrc_r8(REG_H);
  goto after;
prefix_0D:
  rrc_r8(REG_L);
  goto after;
prefix_0E:
  rrc_hl();
  goto after;
prefix_0F:
  rrc_r8(REG_A);
  goto after;
prefix_10:
  rl_r8(REG_B);
  goto after;
prefix_11:
  rl_r8(REG_C);
  goto after;
prefix_12:
  rl_r8(REG_D);
  goto after;
prefix_13:
  rl_r8(REG_E);
  goto after;
prefix_14:
  rl_r8(REG_H);
  goto after;
prefix_15:
  rl_r8(REG_L);
  goto after;
prefix_16:
  rl_hl();
  goto after;
prefix_17:
  rl_r8(REG_A);
  goto after;
prefix_18:
  rr_r8(REG_B);
  goto after;
prefix_19:
  rr_r8(REG_C);
  goto after;
prefix_1A:
  rr_r8(REG_D);
  goto after;
prefix_1B:
  rr_r8(REG_E);
  goto after;
prefix_1C:
  rr_r8(REG_H);
  goto after;
prefix_1D:
  rr_r8(REG_L);
  goto after;
prefix_1E:
  rr_hl();
  goto after;
prefix_1F:
  rr_r8(REG_A);
  goto after;
prefix_20:
  sla_r8(REG_B);
  goto after;
prefix_21:
  sla_r8(REG_C);
  goto after;
prefix_22:
  sla_r8(REG_D);
  goto after;
prefix_23:
  sla_r8(REG_E);
  goto after;
prefix_24:
  sla_r8(REG_H);
  goto after;
prefix_25:
  sla_r8(REG_L);
  goto after;
prefix_26:
  sla_hl();
  goto after;
prefix_27:
  sla_r8(REG_A);
  goto after;
prefix_28:
  sra_r8(REG_B);
  goto after;
prefix_29:
  sra_r8(REG_C);
  goto after;
prefix_2A:
  sra_r8(REG_D);
  goto after;
prefix_2B:
  sra_r8(REG_E);
  goto after;
prefix_2C:
  sra_r8(REG_H);
  goto after;
prefix_2D:
  sra_r8(REG_L);
  goto after;
prefix_2E:
  sra_hl();
  goto after;
prefix_2F:
  sra_r8(REG_A);
  goto after;
prefix_30:
  swap_r8(REG_B);
  goto after;
prefix_31:
  swap_r8(REG_C);
  goto after;
prefix_32:
  swap_r8(REG_D);
  goto after;
prefix_33:
  swap_r8(REG_E);
  goto after;
prefix_34:
  swap_r8(REG_H);
  goto after;
prefix_35:
  swap_r8(REG_L);
  goto after;
prefix_36:
  swap_hl();
  goto after;
prefix_37:
  swap_r8(REG_A);
  goto after;
prefix_38:
  srl_r8(REG_B);
  goto after;
prefix_39:
  srl_r8(REG_C);
  goto after;
prefix_3A:
  srl_r8(REG_D);
  goto after;
prefix_3B:
  srl_r8(REG_E);
  goto after;
prefix_3C:
  srl_r8(REG_H);
  goto after;
prefix_3D:
  srl_r8(REG_L);
  goto after;
prefix_3E:
  srl_hl();
  goto after;
prefix_3F:
  srl_r8(REG_A);
  goto after;
prefix_40:
  bit_u3_r8(0, REG_B);
  goto after;
prefix_41:
  bit_u3_r8(0, REG_C);
  goto after;
prefix_42:
  bit_u3_r8(0, REG_D);
  goto after;
prefix_43:
  bit_u3_r8(0, REG_E);
  goto after;
prefix_44:
  bit_u3_r8(0, REG_H);
  goto after;
prefix_45:
  bit_u3_r8(0, REG_L);
  goto after;
prefix_46:
  bit_u3_hl(0);
  goto after;
prefix_47:
  bit_u3_r8(0, REG_A);
  goto after;
prefix_48:
  bit_u3_r8(1, REG_B);
  goto after;
prefix_49:
  bit_u3_r8(1, REG_C);
  goto after;
prefix_4A:
  bit_u3_r8(1, REG_D);
  goto after;
prefix_4B:
  bit_u3_r8(1, REG_E);
  goto after;
prefix_4C:
  bit_u3_r8(1, REG_H);
  goto after;
prefix_4D:
  bit_u3_r8(1, REG_L);
  goto after;
prefix_4E:
  bit_u3_hl(1);
  goto after;
prefix_4F:
  bit_u3_r8(1, REG_A);
  goto after;
prefix_50:
  bit_u3_r8(2, REG_B);
  goto after;
prefix_51:
  bit_u3_r8(2, REG_C);
  goto after;
prefix_52:
  bit_u3_r8(2, REG_D);
  goto after;
prefix_53:
  bit_u3_r8(2, REG_E);
  goto after;
prefix_54:
  bit_u3_r8(2, REG_H);
  goto after;
prefix_55:
  bit_u3_r8(2, REG_L);
  goto after;
prefix_56:
  bit_u3_hl(2);
  goto after;
prefix_57:
  bit_u3_r8(2, REG_A);
  goto after;
prefix_58:
  bit_u3_r8(3, REG_B);
  goto after;
prefix_59:
  bit_u3_r8(3, REG_C);
  goto after;
prefix_5A:
  bit_u3_r8(3, REG_D);
  goto after;
prefix_5B:
  bit_u3_r8(3, REG_E);
  goto after;
prefix_5C:
  bit_u3_r8(3, REG_H);
  goto after;
prefix_5D:
  bit_u3_r8(3, REG_L);
  goto after;
prefix_5E:
  bit_u3_hl(3);
  goto after;
prefix_5F:
  bit_u3_r8(3, REG_A);
  goto after;
prefix_60:
  bit_u3_r8(4, REG_B);
  goto after;
prefix_61:
  bit_u3_r8(4, REG_C);
  goto after;
prefix_62:
  bit_u3_r8(4, REG_D);
  goto after;
prefix_63:
  bit_u3_r8(4, REG_E);
  goto after;
prefix_64:
  bit_u3_r8(4, REG_H);
  goto after;
prefix_65:
  bit_u3_r8(4, REG_L);
  goto after;
prefix_66:
  bit_u3_hl(4);
  goto after;
prefix_67:
  bit_u3_r8(4, REG_A);
  goto after;
prefix_68:
  bit_u3_r8(5, REG_B);
  goto after;
prefix_69:
  bit_u3_r8(5, REG_C);
  goto after;
prefix_6A:
  bit_u3_r8(5, REG_D);
  goto after;
prefix_6B:
  bit_u3_r8(5, REG_E);
  goto after;
prefix_6C:
  bit_u3_r8(5, REG_H);
  goto after;
prefix_6D:
  bit_u3_r8(5, REG_L);
  goto after;
prefix_6E:
  bit_u3_hl(5);
  goto after;
prefix_6F:
  bit_u3_r8(5, REG_A);
  goto after;
prefix_70:
  bit_u3_r8(6, REG_B);
  goto after;
prefix_71:
  bit_u3_r8(6, REG_C);
  goto after;
prefix_72:
  bit_u3_r8(6, REG_D);
  goto after;
prefix_73:
  bit_u3_r8(6, REG_E);
  goto after;
prefix_74:
  bit_u3_r8(6, REG_H);
  goto after;
prefix_75:
  bit_u3_r8(6, REG_L);
  goto after;
prefix_76:
  bit_u3_hl(6);
  goto after;
prefix_77:
  bit_u3_r8(6, REG_A);
  goto after;
prefix_78:
  bit_u3_r8(7, REG_B);
  goto after;
prefix_79:
  bit_u3_r8(7, REG_C);
  goto after;
prefix_7A:
  bit_u3_r8(7, REG_D);
  goto after;
prefix_7B:
  bit_u3_r8(7, REG_E);
  goto after;
prefix_7C:
  bit_u3_r8(7, REG_H);
  goto after;
prefix_7D:
  bit_u3_r8(7, REG_L);
  goto after;
prefix_7E:
  bit_u3_hl(7);
  goto after;
prefix_7F:
  bit_u3_r8(7, REG_A);
  goto after;
prefix_80:
  res_u3_r8(0, REG_B);
  goto after;
prefix_81:
  res_u3_r8(0, REG_C);
  goto after;
prefix_82:
  res_u3_r8(0, REG_D);
  goto after;
prefix_83:
  res_u3_r8(0, REG_E);
  goto after;
prefix_84:
  res_u3_r8(0, REG_H);
  goto after;
prefix_85:
  res_u3_r8(0, REG_L);
  goto after;
prefix_86:
  res_u3_hl(0);
  goto after;
prefix_87:
  res_u3_r8(0, REG_A);
  goto after;
prefix_88:
  res_u3_r8(1, REG_B);
  goto after;
prefix_89:
  res_u3_r8(1, REG_C);
  goto after;
prefix_8A:
  res_u3_r8(1, REG_D);
  goto after;
prefix_8B:
  res_u3_r8(1, REG_E);
  goto after;
prefix_8C:
  res_u3_r8(1, REG_H);
  goto after;
prefix_8D:
  res_u3_r8(1, REG_L);
  goto after;
prefix_8E:
  res_u3_hl(1);
  goto after;
prefix_8F:
  res_u3_r8(1, REG_A);
  goto after;
prefix_90:
  res_u3_r8(2, REG_B);
  goto after;
prefix_91:
  res_u3_r8(2, REG_C);
  goto after;
prefix_92:
  res_u3_r8(2, REG_D);
  goto after;
prefix_93:
  res_u3_r8(2, REG_E);
  goto after;
prefix_94:
  res_u3_r8(2, REG_H);
  goto after;
prefix_95:
  res_u3_r8(2, REG_L);
  goto after;
prefix_96:
  res_u3_hl(2);
  goto after;
prefix_97:
  res_u3_r8(2, REG_A);
  goto after;
prefix_98:
  res_u3_r8(3, REG_B);
  goto after;
prefix_99:
  res_u3_r8(3, REG_C);
  goto after;
prefix_9A:
  res_u3_r8(3, REG_D);
  goto after;
prefix_9B:
  res_u3_r8(3, REG_E);
  goto after;
prefix_9C:
  res_u3_r8(3, REG_H);
  goto after;
prefix_9D:
  res_u3_r8(3, REG_L);
  goto after;
prefix_9E:
  res_u3_hl(3);
  goto after;
prefix_9F:
  res_u3_r8(3, REG_A);
  goto after;
prefix_A0:
  res_u3_r8(4, REG_B);
  goto after;
prefix_A1:
  res_u3_r8(4, REG_C);
  goto after;
prefix_A2:
  res_u3_r8(4, REG_D);
  goto after;
prefix_A3:
  res_u3_r8(4, REG_E);
  goto after;
prefix_A4:
  res_u3_r8(4, REG_H);
  goto after;
prefix_A5:
  res_u3_r8(4, REG_L);
  goto after;
prefix_A6:
  res_u3_hl(4);
  goto after;
prefix_A7:
  res_u3_r8(4, REG_A);
  goto after;
prefix_A8:
  res_u3_r8(5, REG_B);
  goto after;
prefix_A9:
  res_u3_r8(5, REG_C);
  goto after;
prefix_AA:
  res_u3_r8(5, REG_D);
  goto after;
prefix_AB:
  res_u3_r8(5, REG_E);
  goto after;
prefix_AC:
  res_u3_r8(5, REG_H);
  goto after;
prefix_AD:
  res_u3_r8(5, REG_L);
  goto after;
prefix_AE:
  res_u3_hl(5);
  goto after;
prefix_AF:
  res_u3_r8(5, REG_A);
  goto after;
prefix_B0:
  res_u3_r8(6, REG_B);
  goto after;
prefix_B1:
  res_u3_r8(6, REG_C);
  goto after;
prefix_B2:
  res_u3_r8(6, REG_D);
  goto after;
prefix_B3:
  res_u3_r8(6, REG_E);
  goto after;
prefix_B4:
  res_u3_r8(6, REG_H);
  goto after;
prefix_B5:
  res_u3_r8(6, REG_L);
  goto after;
prefix_B6:
  res_u3_hl(6);
  goto after;
prefix_B7:
  res_u3_r8(6, REG_A);
  goto after;
prefix_B8:
  res_u3_r8(7, REG_B);
  goto after;
prefix_B9:
  res_u3_r8(7, REG_C);
  goto after;
prefix_BA:
  res_u3_r8(7, REG_D);
  goto after;
prefix_BB:
  res_u3_r8(7, REG_E);
  goto after;
prefix_BC:
  res_u3_r8(7, REG_H);
  goto after;
prefix_BD:
  res_u3_r8(7, REG_L);
  goto after;
prefix_BE:
  res_u3_hl(7);
  goto after;
prefix_BF:
  res_u3_r8(7, REG_A);
  goto after;
prefix_C0:
  set_u3_r8(0, REG_B);
  goto after;
prefix_C1:
  set_u3_r8(0, REG_C);
  goto after;
prefix_C2:
  set_u3_r8(0, REG_D);
  goto after;
prefix_C3:
  set_u3_r8(0, REG_E);
  goto after;
prefix_C4:
  set_u3_r8(0, REG_H);
  goto after;
prefix_C5:
  set_u3_r8(0, REG_L);
  goto after;
prefix_C6:
  set_u3_hl(0);
  goto after;
prefix_C7:
  set_u3_r8(0, REG_A);
  goto after;
prefix_C8:
  set_u3_r8(1, REG_B);
  goto after;
prefix_C9:
  set_u3_r8(1, REG_C);
  goto after;
prefix_CA:
  set_u3_r8(1, REG_D);
  goto after;
prefix_CB:
  set_u3_r8(1, REG_E);
  goto after;
prefix_CC:
  set_u3_r8(1, REG_H);
  goto after;
prefix_CD:
  set_u3_r8(1, REG_L);
  goto after;
prefix_CE:
  set_u3_hl(1);
  goto after;
prefix_CF:
  set_u3_r8(1, REG_A);
  goto after;
prefix_D0:
  set_u3_r8(2, REG_B);
  goto after;
prefix_D1:
  set_u3_r8(2, REG_C);
  goto after;
prefix_D2:
  set_u3_r8(2, REG_D);
  goto after;
prefix_D3:
  set_u3_r8(2, REG_E);
  goto after;
prefix_D4:
  set_u3_r8(2, REG_H);
  goto after;
prefix_D5:
  set_u3_r8(2, REG_L);
  goto after;
prefix_D6:
  set_u3_hl(2);
  goto after;
prefix_D7:
  set_u3_r8(2, REG_A);
  goto after;
prefix_D8:
  set_u3_r8(3, REG_B);
  goto after;
prefix_D9:
  set_u3_r8(3, REG_C);
  goto after;
prefix_DA:
  set_u3_r8(3, REG_D);
  goto after;
prefix_DB:
  set_u3_r8(3, REG_E);
  goto after;
prefix_DC:
  set_u3_r8(3, REG_H);
  goto after;
prefix_DD:
  set_u3_r8(3, REG_L);
  goto after;
prefix_DE:
  set_u3_hl(3);
  goto after;
prefix_DF:
  set_u3_r8(3, REG_A);
  goto after;
prefix_E0:
  set_u3_r8(4, REG_B);
  goto after;
prefix_E1:
  set_u3_r8(4, REG_C);
  goto after;
prefix_E2:
  set_u3_r8(4, REG_D);
  goto after;
prefix_E3:
  set_u3_r8(4, REG_E);
  goto after;
prefix_E4:
  set_u3_r8(4, REG_H);
  goto after;
prefix_E5:
  set_u3_r8(4, REG_L);
  goto after;
prefix_E6:
  set_u3_hl(4);
  goto after;
prefix_E7:
  set_u3_r8(4, REG_A);
  goto after;
prefix_E8:
  set_u3_r8(5, REG_B);
  goto after;
prefix_E9:
  set_u3_r8(5, REG_C);
  goto after;
prefix_EA:
  set_u3_r8(5, REG_D);
  goto after;
prefix_EB:
  set_u3_r8(5, REG_E);
  goto after;
prefix_EC:
  set_u3_r8(5, REG_H);
  goto after;
prefix_ED:
  set_u3_r8(5, REG_L);
  goto after;
prefix_EE:
  set_u3_hl(5);
  goto after;
prefix_EF:
  set_u3_r8(5, REG_A);
  goto after;
prefix_F0:
  set_u3_r8(6, REG_B);
  goto after;
prefix_F1:
  set_u3_r8(6, REG_C);
  goto after;
prefix_F2:
  set_u3_r8(6, REG_D);
  goto after;
prefix_F3:
  set_u3_r8(6, REG_E);
  goto after;
prefix_F4:
  set_u3_r8(6, REG_H);
  goto after;
prefix_F5:
  set_u3_r8(6, REG_L);
  goto after;
prefix_F6:
  set_u3_hl(6);
  goto after;
prefix_F7:
  set_u3_r8(6, REG_A);
  goto after;
prefix_F8:
  set_u3_r8(7, REG_B);
  goto after;
prefix_F9:
  set_u3_r8(7, REG_C);
  goto after;
prefix_FA:
  set_u3_r8(7, REG_D);
  goto after;
prefix_FB:
  set_u3_r8(7, REG_E);
  goto after;
prefix_FC:
  set_u3_r8(7, REG_H);
  goto after;
prefix_FD:
  set_u3_r8(7, REG_L);
  goto after;
prefix_FE:
  set_u3_hl(7);
  goto after;
prefix_FF:
  set_u3_r8(7, REG_A);
  goto after;

after:
  AF.second &= 0xF0;
  if (is_last_instr_ei) {
    is_last_instr_ei = false;
  }
  else if (set_ime) {
    ime = 1;
    set_ime = false;
  }

  return (instr_cycles << 2); // convert to T-cycles
}

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
  // printf("reti\n");
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
  // printf("di\n");
}

void Cpu::ei() {
  set_ime = true;
  is_last_instr_ei = true;
  instr_cycles = 1;
  // printf("ei\n");
}

void Cpu::halt() {
  bool interrupts_pending = mmu.read_byte(IE_REG) & mmu.read_byte(IF_REG) & 0x1F;
  if (ime) {
    if (!interrupts_pending) {
      state = HALTED;
    }
    else {
      service_interrupt();
      instr_cycles = 24;
    }
  }
  else {
    if (interrupts_pending) {
      // exit immediately
      halt_bug = true;
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

void Cpu::print_registers() {
  printf("A: %02X ", AF.first);
  printf("F: %02X ", AF.second);
  printf("B: %02X ", BC.first);
  printf("C: %02X ", BC.second);
  printf("D: %02X ", DE.first);
  printf("E: %02X ", DE.second);
  printf("H: %02X ", HL.first);
  printf("L: %02X ", HL.second);
  printf("SP: %04X ", sp);
  printf("PC: %04X [%02X %02X %02X %02X]\n", pc,
         mmu.read_byte(pc), mmu.read_byte(pc + 1), mmu.read_byte(pc + 2),
         mmu.read_byte(pc + 3));;
}
