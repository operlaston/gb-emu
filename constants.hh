#ifndef CONSTANTS_H
#define CONSTANTS_H

// cpu cycles per second
#define CYCLES_PER_SECOND (1048576) // use m-cycles instead of t-cycles (4194304)

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
#define STAT_INTER (1) // aka stat
#define TIMER_INTER (2)
#define SERIAL_INTER (3)
#define JOYPAD_INTER (4)

#endif
