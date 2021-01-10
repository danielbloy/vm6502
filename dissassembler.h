#pragma once

#include <stdint.h>

// Simple dissassembler that just decode opcode and addresses
typedef struct {
    char const * assembler;
    size_t length;
    uint8_t *opcode; // Address of opcode
}  diassembleResult_t;

// The result of a call to dissassemble is only "safe" to use before another call to it
// as some of the pointers returned are to static allocated memory so will get overwritten.
diassembleResult_t dissassemble(uint8_t *ram);

// More nuanced "dissassembler" that tracks all memory accesses and CPU to output trace information.
void instructionBegin();
void memoryRead(uint16_t address, uint8_t value);
void memoryWrite(uint16_t address, uint8_t value);
void instructionCompleted();
void printInstructionTraceInfo(FILE *);

