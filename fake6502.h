#pragma once

// fake6502.c is from http://rubbermallet.org/fake6502.c
// fake6502.h has been created to share definitions

//6502 defines
#define UNDOCUMENTED //when this is defined, undocumented opcodes are handled.
//otherwise, they're simply treated as NOPs.

#define NES_CPUx     //when this is defined, the binary-coded decimal (BCD)
//status flag is not honored by ADC and SBC. the 2A03
//CPU in the Nintendo Entertainment System does not
//support BCD operation.

#define FLAG_CARRY     0x01
#define FLAG_ZERO      0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL   0x08
#define FLAG_BREAK     0x10
#define FLAG_CONSTANT  0x20
#define FLAG_OVERFLOW  0x40
#define FLAG_SIGN      0x80

#define BASE_STACK     0x100

#define LOW_BYTE(v)   ((unsigned char) (v))
#define HIGH_BYTE(v)  ((unsigned char) (((unsigned int) (v)) >> 8))

#define MAX_ADDRESS 65535
#define RAM_SIZE (MAX_ADDRESS + 1)

//6502 CPU registers
uint16_t pc;
uint8_t sp, a, x, y, status;

void reset6502(); // Call this once before you begin execution.
void nmi6502();   // Trigger an NMI in the 6502 core.
void irq6502();   // Trigger a hardware IRQ in the 6502 core.
void step6502();  // Execute a single instruction.