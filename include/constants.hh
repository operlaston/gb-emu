#ifndef CONSTANTS_H
#define CONSTANTS_H

// cpu cycles per second
#define CYCLES_PER_SECOND (4194304) // t-cycles

// timer registers (addresses in memory)
#define DIV_REG (0xFF04) // div timer
#define TIMA_REG (0xFF05) // the address of the timer
#define TMA_REG (0xFF06) // address of the value to reset the timer to
#define TAC_REG (0xFF07) // address of the frequency of the timer

// interrupt registers
#define IF_REG (0xFF0F)
#define IE_REG (0xFFFF)

// graphical hardware registers
#define LCD_CONTROL (0xFF40)
#define LCD_STATUS (0xFF41)
#define LY (0xFF44)
#define LYC (0xFF45)
#define SCY (0xFF42)
#define SCX (0xFF43)
#define WIN_Y (0xFF4A)
#define WIN_X (0xFF4B)

// interrupt bit positions
#define VBLANK_INTER (0)
#define STAT_INTER (1) // lcd interrupt
#define TIMER_INTER (2)
#define SERIAL_INTER (3)
#define JOYPAD_INTER (4)

// memory sections
#define ROM_0_START (0x0000)
#define ROM_0_END (0x3FFF)
#define ROM_1_START (0x4000)
#define ROM_1_END (0x7FFF)
#define VRAM_START (0x8000)
#define VRAM_END (0x9FFF)
#define EXT_RAM_START (0xA000)
#define EXT_RAM_END (0xBFFF)
#define RAM_START (0xC000)
#define RAM_END (0xDFFF)
#define ECHO_RAM_START (0xE000)
#define ECHO_RAM_END (0xFDFF)
#define OAM_START (0xFE00)
#define OAM_END (0xFE9F)
#define UNUSABLE_START (0xFEA0)
#define UNUSABLE_END (0xFEFF)

#endif
