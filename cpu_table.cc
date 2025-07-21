#include "cpu.hh"
#include <cstring>

void Cpu::init_opcode_table() {
  // miscellaneous instructions
  opcode_table[0x0] = [this](){ nop(); };
  opcode_table[0x10] = [this](){ stop(); };
  opcode_table[0x27] = [this](){ daa(); };

  // interrupt related instructions
  opcode_table[0x76] = [this](){ halt(); };
  opcode_table[0xF3] = [this](){ di(); };
  opcode_table[0xFB] = [this](){ ei(); };

  // stack manipulation instructions
  opcode_table[0x31] = [this](){ ld_sp_n16(); };
  opcode_table[0x33] = [this](){ inc_sp(); };
  opcode_table[0x8] = [this](){ ld_n16_sp(); };
  opcode_table[0x39] = [this](){ add_hl_sp(); };
  opcode_table[0x3B] = [this](){ dec_sp(); };
  opcode_table[0xC1] = [this](){ pop_r16(REG_BC); };
  opcode_table[0xD1] = [this](){ pop_r16(REG_DE); };
  opcode_table[0xE1] = [this](){ pop_r16(REG_HL); };
  opcode_table[0xF1] = [this](){ pop_r16(REG_AF); };
  opcode_table[0xC5] = [this](){ push_r16(REG_BC); };
  opcode_table[0xD5] = [this](){ push_r16(REG_DE); };
  opcode_table[0xE5] = [this](){ push_r16(REG_HL); };
  opcode_table[0xF5] = [this](){ push_r16(REG_AF); };

  opcode_table[0xE8] = [this](){ add_sp_e8(); };
  opcode_table[0xF8] = [this](){ ld_hl_sp_e8(); };
  opcode_table[0xF9] = [this](){ ld_sp_hl(); };

  // carry flag instructions
  opcode_table[0x37] = [this](){ scf(); };
  opcode_table[0x3F] = [this](){ ccf(); };

  // jumps and subroutine instructions
  opcode_table[0x20] = [this](){ jr_cc_e8(FLAG_Z, false); };
  opcode_table[0x30] = [this](){ jr_cc_e8(FLAG_C, false); };
  opcode_table[0x18] = [this](){ jr_e8(); };
  opcode_table[0x28] = [this](){ jr_cc_e8(FLAG_Z, true); };
  opcode_table[0x38] = [this](){ jr_cc_e8(FLAG_C, true); };
  opcode_table[0xC0] = [this](){ ret_cc(FLAG_Z, false); };
  opcode_table[0xD0] = [this](){ ret_cc(FLAG_C, false); };
  opcode_table[0xC2] = [this](){ jp_cc_n16(FLAG_Z, false); };
  opcode_table[0xD2] = [this](){ jp_cc_n16(FLAG_C, false); };
  opcode_table[0xC3] = [this](){ jp_n16(); };
  opcode_table[0xC4] = [this](){ call_cc_n16(FLAG_Z, false); };
  opcode_table[0xD4] = [this](){ call_cc_n16(FLAG_C, false); };
  opcode_table[0xC7] = [this](){ rst_vec(0x00); };
  opcode_table[0xD7] = [this](){ rst_vec(0x10); };
  opcode_table[0xE7] = [this](){ rst_vec(0x20); };
  opcode_table[0xF7] = [this](){ rst_vec(0x30); };
  opcode_table[0xC8] = [this](){ ret_cc(FLAG_Z, true); };
  opcode_table[0xD8] = [this](){ ret_cc(FLAG_C, true); };
  opcode_table[0xC9] = [this](){ ret(); };
  opcode_table[0xD9] = [this](){ reti(); };
  opcode_table[0xE9] = [this](){ jp_hl(); };
  opcode_table[0xCA] = [this](){ jp_cc_n16(FLAG_Z, true); };
  opcode_table[0xDA] = [this](){ jp_cc_n16(FLAG_C, true); };
  opcode_table[0xCC] = [this](){ call_cc_n16(FLAG_Z, true); };
  opcode_table[0xDC] = [this](){ call_cc_n16(FLAG_C, true); };
  opcode_table[0xCD] = [this](){ call_n16(); };
  opcode_table[0xCF] = [this](){ rst_vec(0x08); };
  opcode_table[0xDF] = [this](){ rst_vec(0x18); };
  opcode_table[0xEF] = [this](){ rst_vec(0x28); };
  opcode_table[0xFF] = [this](){ rst_vec(0x38); };
 
  // bit shift instructions
  opcode_table[0x7] = [this](){ rlca(); };
  opcode_table[0x17] = [this](){ rla(); };
  opcode_table[0xF] = [this](){ rrca(); };
  opcode_table[0x1F] = [this](){ rra(); };

  // bit flag instructions are all prefixed

  // bitwise logic instructions
  opcode_table[0x2F] = [this](){ cpl(); };

  // 16 bit arithmetic instructions
  opcode_table[0x3] = [this](){ inc_r16(REG_BC); };
  opcode_table[0x13] = [this](){ inc_r16(REG_DE); };
  opcode_table[0x23] = [this](){ inc_r16(REG_HL); };
  opcode_table[0x9] = [this](){ add_hl_r16(REG_BC); };
  opcode_table[0x19] = [this](){ add_hl_r16(REG_DE); };
  opcode_table[0x29] = [this](){ add_hl_r16(REG_HL); };
  opcode_table[0xB] = [this](){ dec_r16(REG_BC); };
  opcode_table[0x1B] = [this](){ dec_r16(REG_DE); };
  opcode_table[0x2B] = [this](){ dec_r16(REG_HL); };
  
  // 8 bit arithmetic instructions
  opcode_table[0x4] = [this](){ inc_r8(REG_B); };
  opcode_table[0x14] = [this](){ inc_r8(REG_D); };
  opcode_table[0x24] = [this](){ inc_r8(REG_H); };
  opcode_table[0x34] = [this](){ inc_hl(); };  
  opcode_table[0x5] = [this](){ dec_r8(REG_B); };
  opcode_table[0x15] = [this](){ dec_r8(REG_D); };
  opcode_table[0x25] = [this](){ dec_r8(REG_H); };
  opcode_table[0x35] = [this](){ dec_hl(); };  
  opcode_table[0xC] = [this](){ inc_r8(REG_C); };  
  opcode_table[0x1C] = [this](){ inc_r8(REG_E); };  
  opcode_table[0x2C] = [this](){ inc_r8(REG_L); };  
  opcode_table[0x3C] = [this](){ inc_r8(REG_A); };  
  opcode_table[0xD] = [this](){ dec_r8(REG_C); };  
  opcode_table[0x1D] = [this](){ dec_r8(REG_E); };  
  opcode_table[0x2D] = [this](){ dec_r8(REG_L); };  
  opcode_table[0x3D] = [this](){ dec_r8(REG_A); };  

  opcode_table[0x80] = [this](){ add_a_r8(REG_B); };  
  opcode_table[0x81] = [this](){ add_a_r8(REG_C); };  
  opcode_table[0x82] = [this](){ add_a_r8(REG_D); };  
  opcode_table[0x83] = [this](){ add_a_r8(REG_E); };  
  opcode_table[0x84] = [this](){ add_a_r8(REG_H); };  
  opcode_table[0x85] = [this](){ add_a_r8(REG_L); };  
  opcode_table[0x86] = [this](){ add_a_hl(); };  
  opcode_table[0x87] = [this](){ add_a_r8(REG_A); };  

  opcode_table[0x88] = [this](){ adc_a_r8(REG_B); };  
  opcode_table[0x89] = [this](){ adc_a_r8(REG_C); };  
  opcode_table[0x8A] = [this](){ adc_a_r8(REG_D); };  
  opcode_table[0x8B] = [this](){ adc_a_r8(REG_E); };  
  opcode_table[0x8C] = [this](){ adc_a_r8(REG_H); };  
  opcode_table[0x8D] = [this](){ adc_a_r8(REG_L); };  
  opcode_table[0x8E] = [this](){ adc_a_hl(); };  
  opcode_table[0x8F] = [this](){ adc_a_r8(REG_A); };  

  opcode_table[0x90] = [this](){ sub_a_r8(REG_B); };  
  opcode_table[0x91] = [this](){ sub_a_r8(REG_C); };  
  opcode_table[0x92] = [this](){ sub_a_r8(REG_D); };  
  opcode_table[0x93] = [this](){ sub_a_r8(REG_E); };  
  opcode_table[0x94] = [this](){ sub_a_r8(REG_H); };  
  opcode_table[0x95] = [this](){ sub_a_r8(REG_L); };  
  opcode_table[0x96] = [this](){ sub_a_hl(); };  
  opcode_table[0x97] = [this](){ sub_a_r8(REG_A); };  

  opcode_table[0x98] = [this](){ sbc_a_r8(REG_B); };  
  opcode_table[0x99] = [this](){ sbc_a_r8(REG_C); };  
  opcode_table[0x9A] = [this](){ sbc_a_r8(REG_D); };  
  opcode_table[0x9B] = [this](){ sbc_a_r8(REG_E); };  
  opcode_table[0x9C] = [this](){ sbc_a_r8(REG_H); };  
  opcode_table[0x9D] = [this](){ sbc_a_r8(REG_L); };  
  opcode_table[0x9E] = [this](){ sbc_a_hl(); };  
  opcode_table[0x9F] = [this](){ sbc_a_r8(REG_A); };  

  opcode_table[0xA0] = [this](){ and_a_r8(REG_B); };  
  opcode_table[0xA1] = [this](){ and_a_r8(REG_C); };  
  opcode_table[0xA2] = [this](){ and_a_r8(REG_D); };  
  opcode_table[0xA3] = [this](){ and_a_r8(REG_E); };  
  opcode_table[0xA4] = [this](){ and_a_r8(REG_H); };  
  opcode_table[0xA5] = [this](){ and_a_r8(REG_L); };  
  opcode_table[0xA6] = [this](){ and_a_hl(); };  
  opcode_table[0xA7] = [this](){ and_a_r8(REG_A); };  

  opcode_table[0xA8] = [this](){ xor_a_r8(REG_B); };  
  opcode_table[0xA9] = [this](){ xor_a_r8(REG_C); };  
  opcode_table[0xAA] = [this](){ xor_a_r8(REG_D); };  
  opcode_table[0xAB] = [this](){ xor_a_r8(REG_E); };  
  opcode_table[0xAC] = [this](){ xor_a_r8(REG_H); };  
  opcode_table[0xAD] = [this](){ xor_a_r8(REG_L); };  
  opcode_table[0xAE] = [this](){ xor_a_hl(); };  
  opcode_table[0xAF] = [this](){ xor_a_r8(REG_A); };  
 
  opcode_table[0xB0] = [this](){ or_a_r8(REG_B); };  
  opcode_table[0xB1] = [this](){ or_a_r8(REG_C); };  
  opcode_table[0xB2] = [this](){ or_a_r8(REG_D); };  
  opcode_table[0xB3] = [this](){ or_a_r8(REG_E); };  
  opcode_table[0xB4] = [this](){ or_a_r8(REG_H); };  
  opcode_table[0xB5] = [this](){ or_a_r8(REG_L); };  
  opcode_table[0xB6] = [this](){ or_a_hl(); };  
  opcode_table[0xB7] = [this](){ or_a_r8(REG_A); };  

  opcode_table[0xB8] = [this](){ cp_a_r8(REG_B); };  
  opcode_table[0xB9] = [this](){ cp_a_r8(REG_C); };  
  opcode_table[0xBA] = [this](){ cp_a_r8(REG_D); };  
  opcode_table[0xBB] = [this](){ cp_a_r8(REG_E); };  
  opcode_table[0xBC] = [this](){ cp_a_r8(REG_H); };  
  opcode_table[0xBD] = [this](){ cp_a_r8(REG_L); };  
  opcode_table[0xBE] = [this](){ cp_a_hl(); };  
  opcode_table[0xBF] = [this](){ cp_a_r8(REG_A); };  

  opcode_table[0xC6] = [this](){ add_a_n8(); };  
  opcode_table[0xD6] = [this](){ sub_a_n8(); };  
  opcode_table[0xE6] = [this](){ and_a_n8(); };  
  opcode_table[0xF6] = [this](){ or_a_n8(); };  
 
  opcode_table[0xCE] = [this](){ adc_a_n8(); };  
  opcode_table[0xDE] = [this](){ sbc_a_n8(); };  
  opcode_table[0xEE] = [this](){ xor_a_n8(); };  
  opcode_table[0xFE] = [this](){ cp_a_n8(); };  

  // load instructions
  opcode_table[0x1] = [this](){ ld_r16_n16(REG_BC); };
  opcode_table[0x11] = [this](){ ld_r16_n16(REG_DE); };
  opcode_table[0x21] = [this](){ ld_r16_n16(REG_HL); };

  opcode_table[0x2] = [this](){ ld_r16_a(REG_BC); };
  opcode_table[0x12] = [this](){ ld_r16_a(REG_DE); };
  opcode_table[0x22] = [this](){ ld_hli_a(); };
  opcode_table[0x32] = [this](){ ld_hld_a(); };
  opcode_table[0x6] = [this](){ ld_r8_n8(REG_B); };
  opcode_table[0x16] = [this](){ ld_r8_n8(REG_D); };
  opcode_table[0x26] = [this](){ ld_r8_n8(REG_H); };
  opcode_table[0x36] = [this](){ ld_hl_n8(); };
  opcode_table[0x46] = [this](){ ld_r8_hl(REG_B); };
  opcode_table[0x56] = [this](){ ld_r8_hl(REG_D); };
  opcode_table[0x66] = [this](){ ld_r8_hl(REG_H); };

  opcode_table[0xA] = [this](){ ld_a_r16(REG_BC); };
  opcode_table[0x1A] = [this](){ ld_a_r16(REG_DE); };
  opcode_table[0x2A] = [this](){ ld_a_hli(); };
  opcode_table[0x3A] = [this](){ ld_a_hld(); };
  opcode_table[0xE] = [this](){ ld_r8_n8(REG_C); };
  opcode_table[0x1E] = [this](){ ld_r8_n8(REG_E); };
  opcode_table[0x2E] = [this](){ ld_r8_n8(REG_L); };
  opcode_table[0x3E] = [this](){ ld_r8_n8(REG_A); };

  opcode_table[0x40] = [this](){ ld_r8_r8(REG_B, REG_B); };
  opcode_table[0x50] = [this](){ ld_r8_r8(REG_D, REG_B); };
  opcode_table[0x60] = [this](){ ld_r8_r8(REG_H, REG_B); };
  opcode_table[0x70] = [this](){ ld_hl_r8(REG_B); };
  opcode_table[0x41] = [this](){ ld_r8_r8(REG_B, REG_C); };
  opcode_table[0x51] = [this](){ ld_r8_r8(REG_D, REG_C); };
  opcode_table[0x61] = [this](){ ld_r8_r8(REG_H, REG_C); };
  opcode_table[0x71] = [this](){ ld_hl_r8(REG_C); };
  opcode_table[0x42] = [this](){ ld_r8_r8(REG_B, REG_D); };
  opcode_table[0x52] = [this](){ ld_r8_r8(REG_D, REG_D); };
  opcode_table[0x62] = [this](){ ld_r8_r8(REG_H, REG_D); };
  opcode_table[0x72] = [this](){ ld_hl_r8(REG_D); };
  opcode_table[0x43] = [this](){ ld_r8_r8(REG_B, REG_E); };
  opcode_table[0x53] = [this](){ ld_r8_r8(REG_D, REG_E); };
  opcode_table[0x63] = [this](){ ld_r8_r8(REG_H, REG_E); };
  opcode_table[0x73] = [this](){ ld_hl_r8(REG_E); };
  opcode_table[0x44] = [this](){ ld_r8_r8(REG_B, REG_H); };
  opcode_table[0x54] = [this](){ ld_r8_r8(REG_D, REG_H); };
  opcode_table[0x64] = [this](){ ld_r8_r8(REG_H, REG_H); };
  opcode_table[0x74] = [this](){ ld_hl_r8(REG_H); };
  opcode_table[0x45] = [this](){ ld_r8_r8(REG_B, REG_L); };
  opcode_table[0x55] = [this](){ ld_r8_r8(REG_D, REG_L); };
  opcode_table[0x65] = [this](){ ld_r8_r8(REG_H, REG_L); };
  opcode_table[0x75] = [this](){ ld_hl_r8(REG_L); };
  opcode_table[0x47] = [this](){ ld_r8_r8(REG_B, REG_A); };
  opcode_table[0x57] = [this](){ ld_r8_r8(REG_D, REG_A); };
  opcode_table[0x67] = [this](){ ld_r8_r8(REG_H, REG_A); };
  opcode_table[0x77] = [this](){ ld_hl_r8(REG_A); };
  opcode_table[0x48] = [this](){ ld_r8_r8(REG_C, REG_B); };
  opcode_table[0x58] = [this](){ ld_r8_r8(REG_E, REG_B); };
  opcode_table[0x68] = [this](){ ld_r8_r8(REG_L, REG_B); };
  opcode_table[0x78] = [this](){ ld_r8_r8(REG_A, REG_B); };
  opcode_table[0x49] = [this](){ ld_r8_r8(REG_C, REG_C); };
  opcode_table[0x59] = [this](){ ld_r8_r8(REG_E, REG_C); };
  opcode_table[0x69] = [this](){ ld_r8_r8(REG_L, REG_C); };
  opcode_table[0x79] = [this](){ ld_r8_r8(REG_A, REG_C); };
  opcode_table[0x4A] = [this](){ ld_r8_r8(REG_C, REG_D); };
  opcode_table[0x5A] = [this](){ ld_r8_r8(REG_E, REG_D); };
  opcode_table[0x6A] = [this](){ ld_r8_r8(REG_L, REG_D); };
  opcode_table[0x7A] = [this](){ ld_r8_r8(REG_A, REG_D); };
  opcode_table[0x4B] = [this](){ ld_r8_r8(REG_C, REG_E); };
  opcode_table[0x5B] = [this](){ ld_r8_r8(REG_E, REG_E); };
  opcode_table[0x6B] = [this](){ ld_r8_r8(REG_L, REG_E); };
  opcode_table[0x7B] = [this](){ ld_r8_r8(REG_A, REG_E); };
  opcode_table[0x4C] = [this](){ ld_r8_r8(REG_C, REG_H); };
  opcode_table[0x5C] = [this](){ ld_r8_r8(REG_E, REG_H); };
  opcode_table[0x6C] = [this](){ ld_r8_r8(REG_L, REG_H); };
  opcode_table[0x7C] = [this](){ ld_r8_r8(REG_A, REG_H); };
  opcode_table[0x4D] = [this](){ ld_r8_r8(REG_C, REG_L); };
  opcode_table[0x5D] = [this](){ ld_r8_r8(REG_E, REG_L); };
  opcode_table[0x6D] = [this](){ ld_r8_r8(REG_L, REG_L); };
  opcode_table[0x7D] = [this](){ ld_r8_r8(REG_A, REG_L); };
  opcode_table[0x4E] = [this](){ ld_r8_hl(REG_C); };
  opcode_table[0x5E] = [this](){ ld_r8_hl(REG_E); };
  opcode_table[0x6E] = [this](){ ld_r8_hl(REG_L); };
  opcode_table[0x7E] = [this](){ ld_r8_hl(REG_A); };
  opcode_table[0x4F] = [this](){ ld_r8_r8(REG_C, REG_A); };
  opcode_table[0x5F] = [this](){ ld_r8_r8(REG_E, REG_A); };
  opcode_table[0x6F] = [this](){ ld_r8_r8(REG_L, REG_A); };
  opcode_table[0x7F] = [this](){ ld_r8_r8(REG_A, REG_A); };

  opcode_table[0xE0] = [this](){ ldh_n8_a(); };
  opcode_table[0xF0] = [this](){ ldh_a_n8(); };
  opcode_table[0xE2] = [this](){ ldh_c_a(); };
  opcode_table[0xF2] = [this](){ ldh_a_c(); };
  opcode_table[0xEA] = [this](){ ld_n16_a(); };
  opcode_table[0xFA] = [this](){ ld_a_n16(); };
}

void Cpu::init_prefix_table() {
  // prefixed instructions
  prefix_table[0x0] = [this](){ rlc_r8(REG_B); };
  prefix_table[0x1] = [this](){ rlc_r8(REG_C); };
  prefix_table[0x2] = [this](){ rlc_r8(REG_D); };
  prefix_table[0x3] = [this](){ rlc_r8(REG_E); };
  prefix_table[0x4] = [this](){ rlc_r8(REG_H); };
  prefix_table[0x5] = [this](){ rlc_r8(REG_L); };
  prefix_table[0x6] = [this](){ rlc_hl(); };
  prefix_table[0x7] = [this](){ rlc_r8(REG_A); };
  prefix_table[0x8] = [this](){ rrc_r8(REG_B); };
  prefix_table[0x9] = [this](){ rrc_r8(REG_C); };
  prefix_table[0xA] = [this](){ rrc_r8(REG_D); };
  prefix_table[0xB] = [this](){ rrc_r8(REG_E); };
  prefix_table[0xC] = [this](){ rrc_r8(REG_H); };
  prefix_table[0xD] = [this](){ rrc_r8(REG_L); };
  prefix_table[0xE] = [this](){ rrc_hl(); };
  prefix_table[0xF] = [this](){ rrc_r8(REG_A); };

  prefix_table[0x10] = [this](){ rl_r8(REG_B); };
  prefix_table[0x11] = [this](){ rl_r8(REG_C); };
  prefix_table[0x12] = [this](){ rl_r8(REG_D); };
  prefix_table[0x13] = [this](){ rl_r8(REG_E); };
  prefix_table[0x14] = [this](){ rl_r8(REG_H); };
  prefix_table[0x15] = [this](){ rl_r8(REG_L); };
  prefix_table[0x16] = [this](){ rl_hl(); };
  prefix_table[0x17] = [this](){ rl_r8(REG_A); };
  prefix_table[0x18] = [this](){ rr_r8(REG_B); };
  prefix_table[0x19] = [this](){ rr_r8(REG_C); };
  prefix_table[0x1A] = [this](){ rr_r8(REG_D); };
  prefix_table[0x1B] = [this](){ rr_r8(REG_E); };
  prefix_table[0x1C] = [this](){ rr_r8(REG_H); };
  prefix_table[0x1D] = [this](){ rr_r8(REG_L); };
  prefix_table[0x1E] = [this](){ rr_hl(); };
  prefix_table[0x1F] = [this](){ rr_r8(REG_A); };

  prefix_table[0x20] = [this](){ sla_r8(REG_B); };
  prefix_table[0x21] = [this](){ sla_r8(REG_C); };
  prefix_table[0x22] = [this](){ sla_r8(REG_D); };
  prefix_table[0x23] = [this](){ sla_r8(REG_E); };
  prefix_table[0x24] = [this](){ sla_r8(REG_H); };
  prefix_table[0x25] = [this](){ sla_r8(REG_L); };
  prefix_table[0x26] = [this](){ sla_hl(); };
  prefix_table[0x27] = [this](){ sla_r8(REG_A); };
  prefix_table[0x28] = [this](){ sra_r8(REG_B); };
  prefix_table[0x29] = [this](){ sra_r8(REG_C); };
  prefix_table[0x2A] = [this](){ sra_r8(REG_D); };
  prefix_table[0x2B] = [this](){ sra_r8(REG_E); };
  prefix_table[0x2C] = [this](){ sra_r8(REG_H); };
  prefix_table[0x2D] = [this](){ sra_r8(REG_L); };
  prefix_table[0x2E] = [this](){ sra_hl(); };
  prefix_table[0x2F] = [this](){ sra_r8(REG_A); };

  prefix_table[0x30] = [this](){ swap_r8(REG_B); };
  prefix_table[0x31] = [this](){ swap_r8(REG_C); };
  prefix_table[0x32] = [this](){ swap_r8(REG_D); };
  prefix_table[0x33] = [this](){ swap_r8(REG_E); };
  prefix_table[0x34] = [this](){ swap_r8(REG_H); };
  prefix_table[0x35] = [this](){ swap_r8(REG_L); };
  prefix_table[0x36] = [this](){ swap_hl(); };
  prefix_table[0x37] = [this](){ swap_r8(REG_A); };
  prefix_table[0x38] = [this](){ srl_r8(REG_B); };
  prefix_table[0x39] = [this](){ srl_r8(REG_C); };
  prefix_table[0x3A] = [this](){ srl_r8(REG_D); };
  prefix_table[0x3B] = [this](){ srl_r8(REG_E); };
  prefix_table[0x3C] = [this](){ srl_r8(REG_H); };
  prefix_table[0x3D] = [this](){ srl_r8(REG_L); };
  prefix_table[0x3E] = [this](){ srl_hl(); };
  prefix_table[0x3F] = [this](){ srl_r8(REG_A); };

  prefix_table[0x40] = [this](){ bit_u3_r8(0, REG_B); };
  prefix_table[0x41] = [this](){ bit_u3_r8(0, REG_C); };
  prefix_table[0x42] = [this](){ bit_u3_r8(0, REG_D); };
  prefix_table[0x43] = [this](){ bit_u3_r8(0, REG_E); };
  prefix_table[0x44] = [this](){ bit_u3_r8(0, REG_H); };
  prefix_table[0x45] = [this](){ bit_u3_r8(0, REG_L); };
  prefix_table[0x46] = [this](){ bit_u3_hl(0); };
  prefix_table[0x47] = [this](){ bit_u3_r8(0, REG_A); };
  prefix_table[0x48] = [this](){ bit_u3_r8(1, REG_B); };
  prefix_table[0x49] = [this](){ bit_u3_r8(1, REG_C); };
  prefix_table[0x4A] = [this](){ bit_u3_r8(1, REG_D); };
  prefix_table[0x4B] = [this](){ bit_u3_r8(1, REG_E); };
  prefix_table[0x4C] = [this](){ bit_u3_r8(1, REG_H); };
  prefix_table[0x4D] = [this](){ bit_u3_r8(1, REG_L); };
  prefix_table[0x4E] = [this](){ bit_u3_hl(1); };
  prefix_table[0x4F] = [this](){ bit_u3_r8(1, REG_A); };

  prefix_table[0x50] = [this](){ bit_u3_r8(2, REG_B); };
  prefix_table[0x51] = [this](){ bit_u3_r8(2, REG_C); };
  prefix_table[0x52] = [this](){ bit_u3_r8(2, REG_D); };
  prefix_table[0x53] = [this](){ bit_u3_r8(2, REG_E); };
  prefix_table[0x54] = [this](){ bit_u3_r8(2, REG_H); };
  prefix_table[0x55] = [this](){ bit_u3_r8(2, REG_L); };
  prefix_table[0x56] = [this](){ bit_u3_hl(2); };
  prefix_table[0x57] = [this](){ bit_u3_r8(2, REG_A); };
  prefix_table[0x58] = [this](){ bit_u3_r8(3, REG_B); };
  prefix_table[0x59] = [this](){ bit_u3_r8(3, REG_C); };
  prefix_table[0x5A] = [this](){ bit_u3_r8(3, REG_D); };
  prefix_table[0x5B] = [this](){ bit_u3_r8(3, REG_E); };
  prefix_table[0x5C] = [this](){ bit_u3_r8(3, REG_H); };
  prefix_table[0x5D] = [this](){ bit_u3_r8(3, REG_L); };
  prefix_table[0x5E] = [this](){ bit_u3_hl(3); };
  prefix_table[0x5F] = [this](){ bit_u3_r8(3, REG_A); };

  prefix_table[0x60] = [this](){ bit_u3_r8(4, REG_B); };
  prefix_table[0x61] = [this](){ bit_u3_r8(4, REG_C); };
  prefix_table[0x62] = [this](){ bit_u3_r8(4, REG_D); };
  prefix_table[0x63] = [this](){ bit_u3_r8(4, REG_E); };
  prefix_table[0x64] = [this](){ bit_u3_r8(4, REG_H); };
  prefix_table[0x65] = [this](){ bit_u3_r8(4, REG_L); };
  prefix_table[0x66] = [this](){ bit_u3_hl(4); };
  prefix_table[0x67] = [this](){ bit_u3_r8(4, REG_A); };
  prefix_table[0x68] = [this](){ bit_u3_r8(5, REG_B); };
  prefix_table[0x69] = [this](){ bit_u3_r8(5, REG_C); };
  prefix_table[0x6A] = [this](){ bit_u3_r8(5, REG_D); };
  prefix_table[0x6B] = [this](){ bit_u3_r8(5, REG_E); };
  prefix_table[0x6C] = [this](){ bit_u3_r8(5, REG_H); };
  prefix_table[0x6D] = [this](){ bit_u3_r8(5, REG_L); };
  prefix_table[0x6E] = [this](){ bit_u3_hl(5); };
  prefix_table[0x6F] = [this](){ bit_u3_r8(5, REG_A); };

  prefix_table[0x70] = [this](){ bit_u3_r8(6, REG_B); };
  prefix_table[0x71] = [this](){ bit_u3_r8(6, REG_C); };
  prefix_table[0x72] = [this](){ bit_u3_r8(6, REG_D); };
  prefix_table[0x73] = [this](){ bit_u3_r8(6, REG_E); };
  prefix_table[0x74] = [this](){ bit_u3_r8(6, REG_H); };
  prefix_table[0x75] = [this](){ bit_u3_r8(6, REG_L); };
  prefix_table[0x76] = [this](){ bit_u3_hl(6); };
  prefix_table[0x77] = [this](){ bit_u3_r8(6, REG_A); };
  prefix_table[0x78] = [this](){ bit_u3_r8(7, REG_B); };
  prefix_table[0x79] = [this](){ bit_u3_r8(7, REG_C); };
  prefix_table[0x7A] = [this](){ bit_u3_r8(7, REG_D); };
  prefix_table[0x7B] = [this](){ bit_u3_r8(7, REG_E); };
  prefix_table[0x7C] = [this](){ bit_u3_r8(7, REG_H); };
  prefix_table[0x7D] = [this](){ bit_u3_r8(7, REG_L); };
  prefix_table[0x7E] = [this](){ bit_u3_hl(7); };
  prefix_table[0x7F] = [this](){ bit_u3_r8(7, REG_A); };

  prefix_table[0x80] = [this](){ res_u3_r8(0, REG_B); };
  prefix_table[0x81] = [this](){ res_u3_r8(0, REG_C); };
  prefix_table[0x82] = [this](){ res_u3_r8(0, REG_D); };
  prefix_table[0x83] = [this](){ res_u3_r8(0, REG_E); };
  prefix_table[0x84] = [this](){ res_u3_r8(0, REG_H); };
  prefix_table[0x85] = [this](){ res_u3_r8(0, REG_L); };
  prefix_table[0x86] = [this](){ res_u3_hl(0); };
  prefix_table[0x87] = [this](){ res_u3_r8(0, REG_A); };
  prefix_table[0x88] = [this](){ res_u3_r8(1, REG_B); };
  prefix_table[0x89] = [this](){ res_u3_r8(1, REG_C); };
  prefix_table[0x8A] = [this](){ res_u3_r8(1, REG_D); };
  prefix_table[0x8B] = [this](){ res_u3_r8(1, REG_E); };
  prefix_table[0x8C] = [this](){ res_u3_r8(1, REG_H); };
  prefix_table[0x8D] = [this](){ res_u3_r8(1, REG_L); };
  prefix_table[0x8E] = [this](){ res_u3_hl(1); };
  prefix_table[0x8F] = [this](){ res_u3_r8(1, REG_A); };

  prefix_table[0x90] = [this](){ res_u3_r8(2, REG_B); };
  prefix_table[0x91] = [this](){ res_u3_r8(2, REG_C); };
  prefix_table[0x92] = [this](){ res_u3_r8(2, REG_D); };
  prefix_table[0x93] = [this](){ res_u3_r8(2, REG_E); };
  prefix_table[0x94] = [this](){ res_u3_r8(2, REG_H); };
  prefix_table[0x95] = [this](){ res_u3_r8(2, REG_L); };
  prefix_table[0x96] = [this](){ res_u3_hl(2); };
  prefix_table[0x97] = [this](){ res_u3_r8(2, REG_A); };
  prefix_table[0x98] = [this](){ res_u3_r8(3, REG_B); };
  prefix_table[0x99] = [this](){ res_u3_r8(3, REG_C); };
  prefix_table[0x9A] = [this](){ res_u3_r8(3, REG_D); };
  prefix_table[0x9B] = [this](){ res_u3_r8(3, REG_E); };
  prefix_table[0x9C] = [this](){ res_u3_r8(3, REG_H); };
  prefix_table[0x9D] = [this](){ res_u3_r8(3, REG_L); };
  prefix_table[0x9E] = [this](){ res_u3_hl(3); };
  prefix_table[0x9F] = [this](){ res_u3_r8(3, REG_A); };

  prefix_table[0xA0] = [this](){ res_u3_r8(4, REG_B); };
  prefix_table[0xA1] = [this](){ res_u3_r8(4, REG_C); };
  prefix_table[0xA2] = [this](){ res_u3_r8(4, REG_D); };
  prefix_table[0xA3] = [this](){ res_u3_r8(4, REG_E); };
  prefix_table[0xA4] = [this](){ res_u3_r8(4, REG_H); };
  prefix_table[0xA5] = [this](){ res_u3_r8(4, REG_L); };
  prefix_table[0xA6] = [this](){ res_u3_hl(4); };
  prefix_table[0xA7] = [this](){ res_u3_r8(4, REG_A); };
  prefix_table[0xA8] = [this](){ res_u3_r8(5, REG_B); };
  prefix_table[0xA9] = [this](){ res_u3_r8(5, REG_C); };
  prefix_table[0xAA] = [this](){ res_u3_r8(5, REG_D); };
  prefix_table[0xAB] = [this](){ res_u3_r8(5, REG_E); };
  prefix_table[0xAC] = [this](){ res_u3_r8(5, REG_H); };
  prefix_table[0xAD] = [this](){ res_u3_r8(5, REG_L); };
  prefix_table[0xAE] = [this](){ res_u3_hl(5); };
  prefix_table[0xAF] = [this](){ res_u3_r8(5, REG_A); };

  prefix_table[0xB0] = [this](){ res_u3_r8(6, REG_B); };
  prefix_table[0xB1] = [this](){ res_u3_r8(6, REG_C); };
  prefix_table[0xB2] = [this](){ res_u3_r8(6, REG_D); };
  prefix_table[0xB3] = [this](){ res_u3_r8(6, REG_E); };
  prefix_table[0xB4] = [this](){ res_u3_r8(6, REG_H); };
  prefix_table[0xB5] = [this](){ res_u3_r8(6, REG_L); };
  prefix_table[0xB6] = [this](){ res_u3_hl(6); };
  prefix_table[0xB7] = [this](){ res_u3_r8(6, REG_A); };
  prefix_table[0xB8] = [this](){ res_u3_r8(7, REG_B); };
  prefix_table[0xB9] = [this](){ res_u3_r8(7, REG_C); };
  prefix_table[0xBA] = [this](){ res_u3_r8(7, REG_D); };
  prefix_table[0xBB] = [this](){ res_u3_r8(7, REG_E); };
  prefix_table[0xBC] = [this](){ res_u3_r8(7, REG_H); };
  prefix_table[0xBD] = [this](){ res_u3_r8(7, REG_L); };
  prefix_table[0xBE] = [this](){ res_u3_hl(7); };
  prefix_table[0xBF] = [this](){ res_u3_r8(7, REG_A); };

  prefix_table[0xC0] = [this](){ set_u3_r8(0, REG_B); };
  prefix_table[0xC1] = [this](){ set_u3_r8(0, REG_C); };
  prefix_table[0xC2] = [this](){ set_u3_r8(0, REG_D); };
  prefix_table[0xC3] = [this](){ set_u3_r8(0, REG_E); };
  prefix_table[0xC4] = [this](){ set_u3_r8(0, REG_H); };
  prefix_table[0xC5] = [this](){ set_u3_r8(0, REG_L); };
  prefix_table[0xC6] = [this](){ set_u3_hl(0); };
  prefix_table[0xC7] = [this](){ set_u3_r8(0, REG_A); };
  prefix_table[0xC8] = [this](){ set_u3_r8(1, REG_B); };
  prefix_table[0xC9] = [this](){ set_u3_r8(1, REG_C); };
  prefix_table[0xCA] = [this](){ set_u3_r8(1, REG_D); };
  prefix_table[0xCB] = [this](){ set_u3_r8(1, REG_E); };
  prefix_table[0xCC] = [this](){ set_u3_r8(1, REG_H); };
  prefix_table[0xCD] = [this](){ set_u3_r8(1, REG_L); };
  prefix_table[0xCE] = [this](){ set_u3_hl(1); };
  prefix_table[0xCF] = [this](){ set_u3_r8(1, REG_A); };

  prefix_table[0xD0] = [this](){ set_u3_r8(2, REG_B); };
  prefix_table[0xD1] = [this](){ set_u3_r8(2, REG_C); };
  prefix_table[0xD2] = [this](){ set_u3_r8(2, REG_D); };
  prefix_table[0xD3] = [this](){ set_u3_r8(2, REG_E); };
  prefix_table[0xD4] = [this](){ set_u3_r8(2, REG_H); };
  prefix_table[0xD5] = [this](){ set_u3_r8(2, REG_L); };
  prefix_table[0xD6] = [this](){ set_u3_hl(2); };
  prefix_table[0xD7] = [this](){ set_u3_r8(2, REG_A); };
  prefix_table[0xD8] = [this](){ set_u3_r8(3, REG_B); };
  prefix_table[0xD9] = [this](){ set_u3_r8(3, REG_C); };
  prefix_table[0xDA] = [this](){ set_u3_r8(3, REG_D); };
  prefix_table[0xDB] = [this](){ set_u3_r8(3, REG_E); };
  prefix_table[0xDC] = [this](){ set_u3_r8(3, REG_H); };
  prefix_table[0xDD] = [this](){ set_u3_r8(3, REG_L); };
  prefix_table[0xDE] = [this](){ set_u3_hl(3); };
  prefix_table[0xDF] = [this](){ set_u3_r8(3, REG_A); };

  prefix_table[0xE0] = [this](){ set_u3_r8(4, REG_B); };
  prefix_table[0xE1] = [this](){ set_u3_r8(4, REG_C); };
  prefix_table[0xE2] = [this](){ set_u3_r8(4, REG_D); };
  prefix_table[0xE3] = [this](){ set_u3_r8(4, REG_E); };
  prefix_table[0xE4] = [this](){ set_u3_r8(4, REG_H); };
  prefix_table[0xE5] = [this](){ set_u3_r8(4, REG_L); };
  prefix_table[0xE6] = [this](){ set_u3_hl(4); };
  prefix_table[0xE7] = [this](){ set_u3_r8(4, REG_A); };
  prefix_table[0xE8] = [this](){ set_u3_r8(5, REG_B); };
  prefix_table[0xE9] = [this](){ set_u3_r8(5, REG_C); };
  prefix_table[0xEA] = [this](){ set_u3_r8(5, REG_D); };
  prefix_table[0xEB] = [this](){ set_u3_r8(5, REG_E); };
  prefix_table[0xEC] = [this](){ set_u3_r8(5, REG_H); };
  prefix_table[0xED] = [this](){ set_u3_r8(5, REG_L); };
  prefix_table[0xEE] = [this](){ set_u3_hl(5); };
  prefix_table[0xEF] = [this](){ set_u3_r8(5, REG_A); };

  prefix_table[0xF0] = [this](){ set_u3_r8(6, REG_B); };
  prefix_table[0xF1] = [this](){ set_u3_r8(6, REG_C); };
  prefix_table[0xF2] = [this](){ set_u3_r8(6, REG_D); };
  prefix_table[0xF3] = [this](){ set_u3_r8(6, REG_E); };
  prefix_table[0xF4] = [this](){ set_u3_r8(6, REG_H); };
  prefix_table[0xF5] = [this](){ set_u3_r8(6, REG_L); };
  prefix_table[0xF6] = [this](){ set_u3_hl(6); };
  prefix_table[0xF7] = [this](){ set_u3_r8(6, REG_A); };
  prefix_table[0xF8] = [this](){ set_u3_r8(7, REG_B); };
  prefix_table[0xF9] = [this](){ set_u3_r8(7, REG_C); };
  prefix_table[0xFA] = [this](){ set_u3_r8(7, REG_D); };
  prefix_table[0xFB] = [this](){ set_u3_r8(7, REG_E); };
  prefix_table[0xFC] = [this](){ set_u3_r8(7, REG_H); };
  prefix_table[0xFD] = [this](){ set_u3_r8(7, REG_L); };
  prefix_table[0xFE] = [this](){ set_u3_hl(7); };
  prefix_table[0xFF] = [this](){ set_u3_r8(7, REG_A); };
}
