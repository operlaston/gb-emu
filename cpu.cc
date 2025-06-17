#include "cpu.hh"
#include <cstdint>
#include <pthread.h>
using namespace std;

Cpu::Cpu(char *rom_path) {
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

  // copy rom into memory
  for (unsigned long i = 0; i < sizeof(mem); i++) {
    mem[i] = rom[i];
  }

  // miscellaneous instructions
  opcode_table[0x0] = [this](){ nop(); };
  opcode_table[0xCB] = [this](){ is_prefix = true; };
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
  opcode_table[0xDC] = [this](){ call_cc_n16(FLAG_Z, true); };
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

  cout << "set up instruction tables and initialized memory" << endl;
}

Cpu::~Cpu() {

}

unsigned char Cpu::read_byte(unsigned short address) const {
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

// gameboy is little endian
unsigned short Cpu::read_word(unsigned short address) const {
  unsigned char lower_byte = read_byte(address);
  unsigned char upper_byte = read_byte(address + 1);
  return (upper_byte << 8) | lower_byte;
}

void Cpu::write_byte(unsigned short address, unsigned char data) {
  // remove later
  if (address == 0xFF02) {
    uint8_t c = read_byte(0xFF01);
    printf("%c", c);
    fflush(stdout);
    data &= ~0x80;
  }

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

  else if (address == DIV_REG) { // 0xFF04 (timer)
    // writing anything to this register resets it to 0
    mem[DIV_REG] = 0;
  }

  else {
    mem[address] = data;
  }
}

void Cpu::handle_banking(unsigned short address, unsigned char data) {
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
  unsigned char data = read_byte(pc);
  pc++;
  return data;
}

unsigned short Cpu::next16() {
  unsigned short data = read_word(pc);
  pc += 2;
  return data;
}

void Cpu::set_flag(int flagbit, bool set) {
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

bool Cpu::get_flag(int flagbit) { 
  unsigned char mask = 1 << flagbit;
  return read_r8(REG_F) & mask;
}

void Cpu::request_interrupt(uint8_t bit) {
  // IE (interrupt enable): 0xFFFF
  // IF (interrupt flag/requested): 0xFF0F

  //TODO
  write_byte(IF_REG, read_byte(IF_REG) | (1 << bit));
}

void Cpu::service_interrupt() {
  // IE (interrupt enable): 0xFFFF
  // IF (interrupt flag/requested): 0xFF0F

  //TODO
  if (!ime) {
    return;
  }

  uint16_t interrupt_address = 0;
  uint8_t interrupt_type = 0;
  uint8_t if_reg = read_byte(IF_REG);
  for(int i = 0; i < 5; i++) {
    uint8_t bitmask = 1;
    bitmask <<= i;
    if (if_reg & bitmask) {
      interrupt_type = i;
      switch(i) {
        case VBLANK_INTER:
          interrupt_address = VBLANK_HANDLER;
          break;
        case LCD_INTER:
          interrupt_address = LCD_HANDLER;
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
  write_byte(--sp, pc >> 8);
  write_byte(--sp, pc & 0xFF);
  
  pc = interrupt_address;
  uint8_t mask = 1 << interrupt_type;
  write_byte(IF_REG, if_reg | mask);
}

void Cpu::update_timers(uint8_t cycles) {
  div_cycles += cycles;
  while (div_cycles >= 0xFF) {
    mem[DIV_REG]++;
    div_cycles -= 0xFF;
  }

  uint8_t tac = read_byte(TAC_REG) & 0x3;
  if (!(tac & 0x4)) { // clock is disabled
    // tima_cycles = 0;
    return;
  }
  uint8_t tima = read_byte(TIMA_REG);
  uint32_t max_tima_cycles = 1024; // 4096 hz where tac == 0
  if (tac == 1) {
    // 262144 hz or increment every 16 cycles
    max_tima_cycles = 16;
  }
  else if (tac == 2) {
    // 65536 hz or increment every 64 cycles
    max_tima_cycles = 64;
  }
  else {
    // 16384 hz of increment every 256 cycles
    max_tima_cycles = 256;
  }
  tima_cycles += cycles;
  while (tima_cycles >= max_tima_cycles) {
    tima_cycles -= max_tima_cycles;
    if (tima == 0xFF) {
      write_byte(TIMA_REG, read_byte(TMA_REG));
      request_interrupt(TIMER_INTER);
    }
    else {
      write_byte(TIMA_REG, tima + 1);
    }
  }
}

void Cpu::update() {
  // max cycles per frame (60 frames per second)
  const int CYCLES_PER_FRAME = CYCLES_PER_SECOND / 60;
  int cycles_this_update = 0;
  while (cycles_this_update < CYCLES_PER_FRAME) {
    // perform a cycle
    uint8_t cycles = fetch_and_execute();
    cycles_this_update += cycles;
    // update timers
    update_timers(cycles);
    // update graphics
    // do interrupts
    service_interrupt();
  }
  // render the screen
}

uint8_t Cpu::fetch_and_execute() {
  unsigned char opcode = read_byte(pc);
  pc++;
  auto& opcode_function = opcode_table[opcode];
  if (is_prefix) {
    opcode_function = prefix_table[opcode];
    is_prefix = false;
  }
  
  if (opcode_function) {
    opcode_function();
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
  return 4; // TBD
}

// it is up to the caller to know whether the register
// they are looking for is a 1 byte register or a 2 byte register
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

/*
 * load instructions
 */

void Cpu::ld_r8_r8(REGISTER reg1, REGISTER reg2) {
  // copy reg2 into reg1
  unsigned char *reg1_ptr = find_r8(reg1);
  unsigned char *reg2_ptr = find_r8(reg2);
  *reg1_ptr = *reg2_ptr;
}

void Cpu::ld_r8_n8(REGISTER r8) {
  // copy n8 into r8 
  unsigned short n8 = read_byte(pc);
  pc++;
  unsigned char *reg = find_r8(r8);
  *reg = n8;
}

void Cpu::ld_r16_n16(REGISTER r16) {
  // copy n16 into r16
  // assumes little endian
  unsigned short n16 = read_word(pc);
  pc += 2;
  unsigned short *reg = find_r16(r16);
  *reg = n16;
}

void Cpu::ld_hl_r8(REGISTER r8) {
  // copy the value in r8 into the byte pointed to by HL
  unsigned short byte_loc = HL.reg;
  write_byte(byte_loc, read_r8(r8));
}

void Cpu::ld_hl_n8() {
  // copy the value in n8 into the byte pointed to by HL
  unsigned char n8 = read_byte(pc);
  pc++;
  unsigned short byte_loc = HL.reg;
  write_byte(byte_loc, n8);
}

void Cpu::ld_r8_hl(REGISTER r8) {
  // copy the value pointed to by HL into r8
  unsigned short byte_loc = HL.reg;
  unsigned char byte_val = read_byte(byte_loc);
  unsigned char *reg = find_r8(r8);
  *reg = byte_val;
}

void Cpu::ld_r16_a(REGISTER r16) {
  // copy the value in register A into the byte pointed to by r16
  unsigned char reg_a = AF.first;
  unsigned short r16_loc = *find_r16(r16);
  write_byte(r16_loc, reg_a);
}

void Cpu::ld_n16_a() {
  // copy the value in register A into the byte at address n16
  unsigned short loc = read_word(pc);
  pc += 2;
  write_byte(loc, AF.first);
}

void Cpu::ldh_n8_a() {
  // copy the value in register A into the byte at address n8
  // provided the address is between 0xFF00 and 0xFFFF
  unsigned char loc = read_byte(pc);
  pc += 1;
  loc = 0xFF00 + loc;
  write_byte(loc, AF.first);
}

void Cpu::ldh_c_a() {
  // copy the value in register A into the byte at address 0xFF00 + C
  write_byte(0xFF00 + BC.second, AF.first);
}

void Cpu::ld_a_r16(REGISTER r16) {
  // copy the byte pointed to by r16 into register A  
  unsigned short *reg = find_r16(r16);
  unsigned char val = read_byte(*reg);
  write_r8(REG_A, val);
}

void Cpu::ld_a_n16() {
  unsigned short n16 = next16();
  write_r8(REG_A, read_byte(n16));
}

void Cpu::ldh_a_n8() {
  // load into register A the data from the address specified by 
  // n8 + 0xFF00
  unsigned short n8 = next8();
  write_r8(REG_A, read_byte(0xFF00 + n8));
}

void Cpu::ldh_a_c() {
  // copy the byte from address 0xFF00 + C into register A
  unsigned char val = 0xFF00 + read_r8(REG_C);
  write_r8(REG_A, val);
}

void Cpu::ld_hli_a() {
  // copy the value in register A into the byte pointed to by HL
  // and increment HL afterwards
  unsigned char val = read_r8(REG_A);
  unsigned short loc = HL.reg;
  write_byte(loc, val);
  HL.reg++;
}

void Cpu::ld_hld_a() {
  // copy A into byte pointed by HL and decrement HL
  unsigned char val = read_r8(REG_A);
  unsigned short loc = HL.reg;
  write_byte(loc, val);
  HL.reg--;
}

void Cpu::ld_a_hld() {
  // copy byte pointed by HL into A and decrement HL after
  unsigned char val = read_byte(HL.reg);
  write_r8(REG_A, val);
  HL.reg--;
}

void Cpu::ld_a_hli() {
  // copy byte pointed by HL into A and increment HL after
  write_r8(REG_A, read_byte(HL.reg));
  HL.reg--;
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
}

void Cpu::adc_a_hl() {
  adc_a_helper(read_byte(HL.reg));
}

void Cpu::adc_a_n8() {
  adc_a_helper(next8());
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
}

void Cpu::add_a_hl() {
  add_a_helper(read_byte(HL.reg));
}

void Cpu::add_a_n8() {
  add_a_helper(next8());
}

void Cpu::cp_a_helper(uint8_t val) {
  set_flag(FLAG_Z, AF.first == val);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (AF.first & 0xF) < (val & 0xF));
  set_flag(FLAG_C, AF.first < val);
}

void Cpu::cp_a_r8(REGISTER r8) {
  cp_a_helper(read_r8(r8));
}

void Cpu::cp_a_hl() {
  cp_a_helper(read_byte(HL.reg));
}

void Cpu::cp_a_n8() {
  cp_a_helper(next8());
}

void Cpu::dec_r8(REGISTER r8) {
  uint8_t *reg = find_r8(r8);
  uint8_t prev = *reg;
  *reg = *reg - 1;
  set_flag(FLAG_Z, *reg == 0);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (prev & 0xF) == 0);
}

void Cpu::dec_hl() {
  uint8_t prev = read_byte(HL.reg);
  write_byte(HL.reg, prev - 1);
  set_flag(FLAG_Z, read_byte(HL.reg) == 0);
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, (prev & 0xF) == 0);
}

void Cpu::inc_r8(REGISTER r8) {
  uint8_t *reg = find_r8(r8);
  uint8_t prev = *reg;
  *reg = *reg + 1;
  set_flag(FLAG_Z, *reg == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, (prev & 0xF) == 0xF);
}

void Cpu::inc_hl() {
  uint8_t prev = read_byte(HL.reg);
  write_byte(HL.reg, prev + 1);
  set_flag(FLAG_Z, read_byte(HL.reg) == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, (prev & 0xF) == 0xF);  
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
}

void Cpu::sbc_a_hl() {
  sbc_a_helper(read_byte(HL.reg));
}

void Cpu::sbc_a_n8() {
  sbc_a_helper(next8());
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
}

void Cpu::sub_a_hl() {
  sub_a_helper(read_byte(HL.reg));
}

void Cpu::sub_a_n8() {
  sub_a_helper(next8());
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
}

void Cpu::dec_r16(REGISTER r16) {
  uint16_t *reg = find_r16(r16);
  *reg -= 1;
}

void Cpu::inc_r16(REGISTER r16) {
  uint16_t *reg = find_r16(r16);
  *reg += 1;
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
}

void Cpu::and_a_hl() {
  and_a_helper(read_byte(HL.reg));
}

void Cpu::and_a_n8() {
  and_a_helper(next8());
}

void Cpu::cpl() {
  AF.first = ~AF.first;
  set_flag(FLAG_N, 1);
  set_flag(FLAG_H, 1);
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
}

void Cpu::or_a_hl() {
  or_a_helper(read_byte(HL.reg));
}

void Cpu::or_a_n8() {
  or_a_helper(next8());
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
}

void Cpu::xor_a_hl() {
  xor_a_helper(read_byte(HL.reg));
}

void Cpu::xor_a_n8() {
  xor_a_helper(next8());
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
}

void Cpu::bit_u3_hl(uint8_t bit) {
  uint8_t mask = 1 << bit;
  uint8_t val = read_byte(HL.reg);
  set_flag(FLAG_Z, (val & mask) == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 1);
}

void Cpu::res_u3_r8(uint8_t bit, REGISTER r8) {
  uint8_t mask = 1 << bit;
  mask = ~mask;
  uint8_t *reg = find_r8(r8);
  *reg &= mask;
}

void Cpu::res_u3_hl(uint8_t bit) {
  uint8_t mask = 1 << bit;
  mask = ~mask;
  write_byte(HL.reg, read_byte(HL.reg) & mask);
}

void Cpu::set_u3_r8(uint8_t bit, REGISTER r8) {
  uint8_t mask = 1 << bit;
  uint8_t *reg = find_r8(r8);
  *reg |= mask;
}

void Cpu::set_u3_hl(uint8_t bit) {
  uint8_t mask = 1 << bit;
  write_byte(HL.reg, read_byte(HL.reg) | mask);
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
}

void Cpu::rl_hl() {
  uint8_t val = read_byte(HL.reg);
  uint8_t carry_flag = get_flag(FLAG_C) ? 1 : 0;
  set_flag(FLAG_C, val & 0x80); //most significant bit
  val <<= 1;
  val += carry_flag;
  write_byte(HL.reg, val);

  set_shift_flags(val);
}

void Cpu::rla() {
  uint8_t val = AF.first;
  uint8_t carry_flag = get_flag(FLAG_C) ? 1 : 0;
  set_flag(FLAG_C, val & 0x80); //most significant bit
  val <<= 1;
  val += carry_flag;
  AF.first = val;

  set_shift_flags(1); // RESET zero flag
}

void Cpu::rlc_r8(REGISTER r8) {
  uint8_t reg = read_r8(r8);
  uint8_t msb = reg & 0x80 ? 1 : 0;
  set_flag(FLAG_C, msb); //most significant bit
  reg <<= 1;
  reg += msb;
  write_r8(r8, reg);

  set_shift_flags(reg);
}

void Cpu::rlc_hl() {
  uint8_t val = read_byte(HL.reg);
  uint8_t msb = val & 0x80 ? 1 : 0;
  set_flag(FLAG_C, msb); //most significant bit
  val <<= 1;
  val += msb;
  write_byte(HL.reg, val);

  set_shift_flags(val);
}

void Cpu::rlca() {
  uint8_t val = AF.first;
  uint8_t msb = val & 0x80 ? 1 : 0;
  set_flag(FLAG_C, msb); //most significant bit
  val <<= 1;
  val += msb;
  AF.first = val;

  set_shift_flags(1); // RESET zero flag
}

void Cpu::rr_r8(REGISTER r8) {
  uint8_t reg = read_r8(r8);
  uint8_t carry_flag = get_flag(FLAG_C) ? 0x80 : 0;
  set_flag(FLAG_C, reg & 0x1); //least significant bit
  reg >>= 1;
  reg |= carry_flag;
  write_r8(r8, reg);

  set_shift_flags(reg);
}

void Cpu::rr_hl() {
  uint8_t val = read_byte(HL.reg);
  uint8_t carry_flag = get_flag(FLAG_C) ? 0x80 : 0;
  set_flag(FLAG_C, val & 0x1); //least significant bit
  val >>= 1;
  val |= carry_flag;
  write_byte(HL.reg, val);

  set_shift_flags(val);
}

void Cpu::rra() {
  uint8_t val = AF.first;
  uint8_t carry_flag = get_flag(FLAG_C) ? 0x80 : 0;
  set_flag(FLAG_C, val & 0x1); //least significant bit
  val >>= 1;
  val |= carry_flag;
  AF.first = val;
  set_shift_flags(1); // RESET zero flag
}

void Cpu::rrc_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  uint8_t lsb = val & 0x1 ? 0x80 : 0;
  set_flag(FLAG_C, lsb); //least significant bit
  val >>= 1;
  val |= lsb;
  write_r8(r8, val);
  set_shift_flags(val);
}

void Cpu::rrc_hl() {
  uint8_t val = read_byte(HL.reg);
  uint8_t lsb = val & 0x1 ? 0x80 : 0;
  set_flag(FLAG_C, lsb); //least significant bit
  val >>= 1;
  val |= lsb;
  write_byte(HL.reg, val);
  set_shift_flags(val);
}

void Cpu::rrca() {
  uint8_t val = AF.first;
  uint8_t lsb = val & 0x1 ? 0x80 : 0;
  set_flag(FLAG_C, lsb); //least significant bit
  val >>= 1;
  val |= lsb;
  AF.first = val;
  set_shift_flags(1); //RESET zero flag
}

void Cpu::sla_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  set_flag(FLAG_C, val & 0x80);
  val <<= 1;
  write_r8(r8, val);
  set_shift_flags(val);
}

void Cpu::sla_hl() {
  uint8_t val = read_byte(HL.reg);
  set_flag(FLAG_C, val & 0x80);
  val <<= 1;
  write_byte(HL.reg, val);
  set_shift_flags(val);
}

void Cpu::sra_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  set_flag(FLAG_C, val & 0x1);
  uint8_t mask = val & 0x80; // msb
  val >>= 1;
  val |= mask;
  write_r8(r8, val);
  set_shift_flags(val);
}

void Cpu::sra_hl() {
  uint8_t val = read_byte(HL.reg);
  set_flag(FLAG_C, val & 0x1);
  uint8_t mask = val & 0x80; // msb
  val >>= 1;
  val |= mask;
  write_byte(HL.reg, val);
  set_shift_flags(val);
}

void Cpu::srl_r8(REGISTER r8) {
  uint8_t val = read_r8(r8);
  set_flag(FLAG_C, val & 0x1);
  val >>= 1;
  write_r8(r8, val);
  set_shift_flags(val);
}

void Cpu::srl_hl() {
  uint8_t val = read_byte(HL.reg);
  set_flag(FLAG_C, val & 0x1);
  val >>= 1;
  write_byte(HL.reg, val);
  set_shift_flags(val);
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
}

void Cpu::swap_hl() {
  uint8_t val = read_byte(HL.reg);
  uint8_t lower_four = val & 0xF;
  lower_four <<= 4;
  val >>= 4;
  val |= lower_four;
  write_byte(HL.reg, val);
  set_flag(FLAG_Z, val == 0);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
  set_flag(FLAG_C, 0);
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
  write_byte(--sp, pc >> 8);
  write_byte(--sp, pc & 0xFF);
  
  pc = n16;
}

void Cpu::call_cc_n16(int flag, bool condition) {
  if (get_flag(flag) == condition) {
    call_n16();
  }
}

void Cpu::jp_hl() {
  pc = HL.reg;
}

void Cpu::jp_n16() {
  uint16_t address = next16();
  pc = address;
}

void Cpu::jp_cc_n16(int flag, bool condition) {
  if (get_flag(flag) == condition) {
    jp_n16();
  }
}

void Cpu::jr_e8() {
  int8_t e8 = (int8_t)next8();
  pc += e8;
}

void Cpu::jr_cc_e8(int flag, bool condition) {
  if( get_flag(flag) == condition ) {
    jr_e8();
  }
}

void Cpu::ret() {
  pc = read_word(sp);
  sp += 2;
}

void Cpu::ret_cc(int flag, bool condition) {
  if (get_flag(flag) == condition) {
    ret();
  }
}

void Cpu::reti() {
  ret();
  ime = 1;
}

void Cpu::rst_vec(uint8_t n) {
  uint16_t addr = n | 0x0000;
  // pc is already at the next instruction
  write_byte(--sp, pc >> 8); // most significant byte
  write_byte(--sp, pc & 0xFF);
  pc = addr;
}

/*
 * carry flag instructions
 */

void Cpu::ccf() {
  set_flag(FLAG_C, !get_flag(FLAG_C));
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
}

void Cpu::scf() {
  set_flag(FLAG_C, 1);
  set_flag(FLAG_N, 0);
  set_flag(FLAG_H, 0);
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
}

void Cpu::dec_sp() {
  sp--;
}

void Cpu::inc_sp() {
  sp++;
}

void Cpu::ld_sp_n16() {
  // copy n16 into sp 
  sp = next16();
}

void Cpu::ld_n16_sp() {
  // copy sp into n16 and n16 + 1
  // little endian so write least significant byte first
  unsigned short n16 = next16();
  write_byte(n16, sp & 0xFF); 
  write_byte(n16 + 1, sp >> 8);
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
}

void Cpu::ld_sp_hl() {
  // copy HL into sp
  sp = HL.reg;
}

void Cpu::pop_af() {
  AF.second = read_byte(sp++);
  AF.first = read_byte(sp++);
}

void Cpu::pop_r16(REGISTER r16) {
  uint8_t low = read_byte(sp++);
  uint8_t high = read_byte(sp++);
  uint16_t val = (high << 8) | low;
  write_r16(r16, val);
}

void Cpu::push_af() {
  write_byte(--sp, AF.first);
  write_byte(--sp, AF.second);
}

void Cpu::push_r16(REGISTER r16) {
  uint16_t val = read_r16(r16);
  uint8_t high = val & 0xFF00;
  uint8_t low = val & 0x00FF;
  write_byte(--sp, high);
  write_byte(--sp, low);
}


/*
 * interrupt related instructions
 */

void Cpu::di() {
  ime = 0;
}

void Cpu::ei() {
  set_ime = true;
  is_last_instr_ei = true;
}

void Cpu::halt() {
  //TODO
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
}

void Cpu::nop() {
  // do nothing
  return;
}

void Cpu::stop() {
  //TODO
}
