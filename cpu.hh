#ifndef CPU_H
#define CPU_H
#include <cstring>
#include <stdio.h>
#include <stdint.h>
#include <iostream>

// flags (F register)
#define FLAG_Z (7) // zero flag
#define FLAG_N (6) // subtract flag
#define FLAG_H (5) // half carry flag
#define FLAG_C (4) // carry flag

// cpu cycles per second
#define CYCLES_PER_SECOND (4194304)

// timer registers (addresses in memory)
#define DIV_REG (0xFF04) // div timer
#define TIMA_REG (0xFF05) // the address of the timer
#define TMA_REG (0xFF06) // address of the value to reset the timer to
#define TAC_REG (0xFF07) // address of the frequency of the timer

// interrupt registers
#define IF_REG (0xFF0F)
#define IE_REG (0xFFFF)

// interrupt bit positions
#define VBLANK_INTER (0)
#define LCD_INTER (1) // aka stat
#define TIMER_INTER (2)
#define SERIAL_INTER (3)
#define JOYPAD_INTER (4)

// interrupt mem addresses
#define VBLANK_HANDLER (0x40)
#define LCD_HANDLER (0x48) // aka stat
#define TIMER_HANDLER (0x50)
#define SERIAL_HANDLER (0x58)
#define JOYPAD_HANDLER (0x60)

enum banking_types {
  MBC1,
  MBC2,
  NONE
};

typedef enum {
  REG_A,
  REG_B,
  REG_C,
  REG_D,
  REG_E,
  REG_F,
  REG_H,
  REG_L,
  REG_AF,
  REG_BC,
  REG_DE,
  REG_HL
} REGISTER;

// typedef struct {
//   unsigned char operand_size;
//   std::function<void()> execute_function;
// } Instruction;

union reg_t{
  unsigned char second;
  unsigned char first;
  unsigned short reg;
};

class Cpu { 
  unsigned char mem[0x10000];
  unsigned char rom[0x200000];
  unsigned char num_rom_banks; // rom banks are 16KiB in size
  unsigned char num_ram_banks; // ram banks are 8KiB in size
  enum banking_types rom_banking_type;
  unsigned char curr_rom_bank;
  unsigned char curr_ram_bank;
  bool ram_enabled;
  unsigned char ram_banks[0x8000]; // a ram bank is 0x2000 in size and there are 4 max
  std::function<void()> opcode_table[256];
  std::function<void()> prefix_table[256];


  // first letter is high and second is low
  // e.g. for AF, the higher half is A and the lower half is F
  // ^^ for little endian systems
  reg_t AF; 
  reg_t BC;
  reg_t DE;
  reg_t HL;

  unsigned short sp;
  unsigned short pc;

  unsigned char screen[160][144][3]; // r/g/b for color
  bool ime; // ime (interrupt) flag
  bool set_ime; // set by the EI instruction
  bool is_last_instr_ei;
  bool is_prefix; // set by prefix instruction opcode 0xCB


  void handle_banking(unsigned short address, unsigned char data);
  void write_byte(unsigned short address, unsigned char data);
  void write_r8(REGISTER, unsigned char);
  void write_r16(REGISTER, unsigned short);
  unsigned char read_byte(unsigned short address) const;
  unsigned short read_word(unsigned short address) const;
  unsigned char read_r8(REGISTER);
  unsigned short read_r16(REGISTER);
  unsigned char *find_r8(REGISTER);
  unsigned short *find_r16(REGISTER);
  unsigned char next8();
  unsigned short next16();
  void set_flag(int, bool);
  bool get_flag(int);
  void request_interrupt(uint8_t bit);
  void service_interrupt();

  // timer handling
  void update_timers(uint8_t);
  uint16_t div_cycles;
  uint16_t tima_cycles;
  // uint8_t div;
  // uint8_t tima;
  // uint8_t tma;
  // uint8_t tac;
  
  // load instructions
  void ld_r8_r8(REGISTER, REGISTER);
  void ld_r8_n8(REGISTER);
  void ld_r16_n16(REGISTER);
  void ld_hl_r8(REGISTER);
  void ld_hl_n8();
  void ld_r8_hl(REGISTER);
  void ld_r16_a(REGISTER);
  void ld_n16_a();
  void ldh_n8_a(); // website labelled this wrong (rgbds website)
  void ldh_c_a();
  void ld_a_r16(REGISTER);
  void ld_a_n16();
  void ldh_a_n8(); // website labelled this wrong too
  void ldh_a_c();
  void ld_hli_a();
  void ld_hld_a();
  void ld_a_hli();
  void ld_a_hld();

  // 8 bit arithmetic instructions
  void adc_a_r8(REGISTER);
  void adc_a_hl();
  void adc_a_n8();
  void add_a_r8(REGISTER);
  void add_a_hl();
  void add_a_n8();
  void cp_a_r8(REGISTER);
  void cp_a_hl();
  void cp_a_n8();
  void dec_r8(REGISTER);
  void dec_hl();
  void inc_r8(REGISTER);
  void inc_hl();
  void sbc_a_r8(REGISTER);
  void sbc_a_hl();
  void sbc_a_n8();
  void sub_a_r8(REGISTER);
  void sub_a_hl();
  void sub_a_n8();

  // 16 bit arithmetic instructions
  void add_hl_r16(REGISTER);
  void dec_r16(REGISTER);
  void inc_r16(REGISTER);

  // bitwise logic instructions
  void and_a_r8(REGISTER);
  void and_a_hl();
  void and_a_n8();
  void cpl();
  void or_a_r8(REGISTER);
  void or_a_hl();
  void or_a_n8();
  void xor_a_r8(REGISTER);
  void xor_a_hl();
  void xor_a_n8();

  // bit flag instructions
  void bit_u3_r8(uint8_t, REGISTER);
  void bit_u3_hl(uint8_t);
  void res_u3_r8(uint8_t, REGISTER);
  void res_u3_hl(uint8_t);
  void set_u3_r8(uint8_t, REGISTER);
  void set_u3_hl(uint8_t);

  // bit shift instructions
  void rl_r8(REGISTER);
  void rl_hl();
  void rla();
  void rlc_r8(REGISTER);
  void rlc_hl();
  void rlca();
  void rr_r8(REGISTER);
  void rr_hl();
  void rra();
  void rrc_r8(REGISTER);
  void rrc_hl();
  void rrca();
  void sla_r8(REGISTER);
  void sla_hl();
  void sra_r8(REGISTER);
  void sra_hl();
  void srl_r8(REGISTER);
  void srl_hl();
  void swap_r8(REGISTER);
  void swap_hl();

  // jumps and subroutine instructions 
  void call_n16();
  void call_cc_n16(int flag, bool condition);
  void jp_hl();
  void jp_n16();
  void jp_cc_n16(int flag, bool condition);
  void jr_e8();
  void jr_cc_e8(int flag, bool condition);
  void ret();
  void ret_cc(int flag, bool condition);
  void reti();
  void rst_vec(uint8_t);

  // carry flag instructions
  void ccf();
  void scf();

  // stack manipulation instructions
  void add_hl_sp();
  void add_sp_e8();
  void dec_sp();
  void inc_sp();
  void ld_sp_n16();
  void ld_n16_sp();
  void ld_hl_sp_e8();
  void ld_sp_hl();
  void pop_af();
  void pop_r16(REGISTER);
  void push_af();
  void push_r16(REGISTER);

  // interrupt-related instructions
  void di();
  void ei();
  void halt();
  
  // misc instructions
  void daa();
  void nop();
  void stop();

  // instruction helpers
  void adc_a_helper(uint8_t);
  void add_a_helper(uint8_t);
  void cp_a_helper(uint8_t);
  void sbc_a_helper(uint8_t);
  void sub_a_helper(uint8_t);
  void and_a_helper(uint8_t);
  void or_a_helper(uint8_t);
  void xor_a_helper(uint8_t);
  void set_shift_flags(uint8_t);

public: 
  Cpu(char *rom_path);
  ~Cpu();
  void update();
  uint8_t fetch_and_execute();
};

#endif
