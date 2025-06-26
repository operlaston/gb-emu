#include "timer.hh"
#include "constants.hh"

Timer::Timer(Memory& m) : mmu(m){
  div = 0;
  tima = 0;
  tma = 0;
  tac = 0;
  tima_overflow = -1;
  prev_and_result = false;
}

uint8_t Timer::timer_read(uint16_t reg) {
  switch (reg) {
    case DIV_REG:
      return (div >> 8) & 0xFF;
    case TIMA_REG:
      return tima;
    case TMA_REG:
      return tma;
    case TAC_REG:
      return tac;
  }

  return 0;
}

void Timer::timer_write(uint16_t reg, uint8_t data) {
  switch(reg) {
    case DIV_REG:
      div = 0;
      break;
    case TIMA_REG:
      tima = data;
      break;
    case TMA_REG:
      tma = data;
      break;
    case TAC_REG:
      tac = data;
      break;
  }
}

void Timer::tick() {
  // get the bit number by indexing the lower 2 bits of TAC
  uint8_t bit = bit_masks[tac & 3];
  uint8_t div_bit = (div >> bit) & 1;
  uint8_t enable_bit = (tac >> 2) & 1;
  bool curr_and_result = div_bit & enable_bit;
  if (prev_and_result && !curr_and_result) {
    tima++;
    if (tima == 0) {
      tima = tma;
      mmu.request_interrupt(TIMER_INTER);
    }
  }
  prev_and_result = curr_and_result;

  div++;
}
