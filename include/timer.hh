#ifndef TIMER_H
#define TIMER_H

#include <cstdint>
#include "memory.hh"

class Timer {
  uint16_t div;
  uint8_t tima;
  uint8_t tma;
  uint8_t tac;
  bool prev_and_result;
  int8_t tima_overflow;
  Memory& mmu;

  uint16_t bit_masks[4] = {
    9,
    3,
    5,
    7 
  };

public:
  Timer(Memory& m);
  uint8_t timer_read(uint16_t reg);
  void timer_write(uint16_t reg, uint8_t data);
  void tick();
};

#endif
