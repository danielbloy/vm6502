#pragma ide diagnostic ignored "hicpp-signed-bitwise"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "dissassembler.h"
#include "fake6502.h"

#define STR_SIZE 32

typedef struct {
    char const * address; // String representation of the address
    size_t length; // Number of bytes require to the address in machine code.
} addresstableResult_t;

static addresstableResult_t imp() { //implied
    return (addresstableResult_t){"", 0};
}

static addresstableResult_t acc() { //accumulator
    return (addresstableResult_t){"", 0};
}

static addresstableResult_t imm(uint8_t *ram) { //immediate
    static char address[STR_SIZE] = "";
    static const char*const pattern = "#$%02x";
    snprintf(address, STR_SIZE, pattern, ram[0]);
    return (addresstableResult_t){address, 1};
}

static addresstableResult_t zp(uint8_t *ram) { //zero-page
    static char address[STR_SIZE] = "";
    static const char*const pattern = "$%02x";
    snprintf(address, STR_SIZE, pattern, ram[0]);
    static const addresstableResult_t result = {address, 1};
    return result;
}

static addresstableResult_t zpx(uint8_t *ram) { //zero-page,X
    static char address[STR_SIZE] = "";
    static const char*const pattern = "$%02x,X";
    snprintf(address, STR_SIZE, pattern, ram[0]);
    return (addresstableResult_t){address, 1};
}

static addresstableResult_t zpy(uint8_t *ram) { //zero-page,Y
    static char address[STR_SIZE] = "";
    static const char*const pattern = "$%02x,Y";
    snprintf(address, STR_SIZE, pattern, ram[0]);
    return (addresstableResult_t){address, 1};
}

static addresstableResult_t rel(uint8_t *ram) { //relative for branch ops (8-bit immediate value, sign-extended)
    static char address[STR_SIZE] = "";
    static const char*const pattern = "$%02x ; %4d decimal ";

    const uint8_t operand = ram[0];
    const bool negative = (operand & 0x80) != 0; // If the last bit is set this is negative.
    const uint8_t value = operand & 0x7F;        // All bits except the sign.
    const int decimal = negative ? (value - 128) : value;
    snprintf(address, STR_SIZE, pattern, ram[0], decimal);
    return (addresstableResult_t){address, 1};
}

static addresstableResult_t abso(uint8_t *ram) { //absolute
    static char address[STR_SIZE] = "";
    static const char*const pattern = "$%02x%02x";
    snprintf(address, STR_SIZE, pattern, ram[1], ram[0]);
    return (addresstableResult_t){address, 2};
}

static addresstableResult_t absx(uint8_t *ram) { //absolute,X
    static char address[STR_SIZE] = "";
    static const char*const pattern = "$%02x%02x,X";
    snprintf(address, STR_SIZE, pattern, ram[1], ram[0]);
    return (addresstableResult_t){address, 2};
}

static addresstableResult_t absy(uint8_t *ram) { //absolute,Y
    static char address[STR_SIZE] = "";
    static const char*const pattern = "$%02x%02x,Y";
    snprintf(address, STR_SIZE, pattern, ram[1], ram[0]);
    return (addresstableResult_t){address, 2};
}

static addresstableResult_t ind(uint8_t *ram) { //indirect
    static char address[STR_SIZE] = "";
    static const char*const pattern = "($%02x%02x)";
    snprintf(address, STR_SIZE, pattern, ram[1], ram[0]);
    return (addresstableResult_t){address, 2};
}

static addresstableResult_t indx(uint8_t *ram) { // (indirect,X)
    static char address[STR_SIZE] = "";
    static const char*const pattern = "($%02x,X)";
    snprintf(address, STR_SIZE, pattern, ram[0]);
    return (addresstableResult_t){address, 2};
}

static addresstableResult_t indy(uint8_t *ram) { // (indirect),Y
    static char address[STR_SIZE] = "";
    static const char*const pattern = "($%02x),Y";
    snprintf(address, STR_SIZE, pattern, ram[0]);
    return (addresstableResult_t){address, 2};
}

static addresstableResult_t (*addrtable[256])(uint8_t *) = {
/*        |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |  8  |  9  |  A  |  B  |  C  |  D  |  E  |  F  |     */
/* 0 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 0 */
/* 1 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 1 */
/* 2 */    abso, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 2 */
/* 3 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 3 */
/* 4 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm, abso, abso, abso, abso, /* 4 */
/* 5 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 5 */
/* 6 */     imp, indx,  imp, indx,   zp,   zp,   zp,   zp,  imp,  imm,  acc,  imm,  ind, abso, abso, abso, /* 6 */
/* 7 */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* 7 */
/* 8 */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* 8 */
/* 9 */     rel, indy,  imp, indy,  zpx,  zpx,  zpy,  zpy,  imp, absy,  imp, absy, absx, absx, absy, absy, /* 9 */
/* A */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* A */
/* B */     rel, indy,  imp, indy,  zpx,  zpx,  zpy,  zpy,  imp, absy,  imp, absy, absx, absx, absy, absy, /* B */
/* C */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* C */
/* D */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx, /* D */
/* E */     imm, indx,  imm, indx,   zp,   zp,   zp,   zp,  imp,  imm,  imp,  imm, abso, abso, abso, abso, /* E */
/* F */     rel, indy,  imp, indy,  zpx,  zpx,  zpx,  zpx,  imp, absy,  imp, absy, absx, absx, absx, absx  /* F */
};

static char const * opcodes[256] = {
/*     |  0   |  1   |  2   |  3   |  4   |  5   |  6   |  7   |  8   |  9   |  A   |  B   |  C   |  D   |  E   |  F   |     */
/* 0 */ "brk", "ora", "nop", "slo", "nop", "ora", "asl", "slo", "php", "ora", "asl", "nop", "nop", "ora", "asl", "slo", /* 0 */
/* 1 */ "bpl", "ora", "nop", "slo", "nop", "ora", "asl", "slo", "clc", "ora", "nop", "slo", "nop", "ora", "asl", "slo", /* 1 */
/* 2 */ "jsr", "and", "nop", "rla", "bit", "and", "rol", "rla", "plp", "and", "rol", "nop", "bit", "and", "rol", "rla", /* 2 */
/* 3 */ "bmi", "and", "nop", "rla", "nop", "and", "rol", "rla", "sec", "and", "nop", "rla", "nop", "and", "rol", "rla", /* 3 */
/* 4 */ "rti", "eor", "nop", "sre", "nop", "eor", "lsr", "sre", "pha", "eor", "lsr", "nop", "jmp", "eor", "lsr", "sre", /* 4 */
/* 5 */ "bvc", "eor", "nop", "sre", "nop", "eor", "lsr", "sre", "cli", "eor", "nop", "sre", "nop", "eor", "lsr", "sre", /* 5 */
/* 6 */ "rts", "adc", "nop", "rra", "nop", "adc", "ror", "rra", "pla", "adc", "ror", "nop", "jmp", "adc", "ror", "rra", /* 6 */
/* 7 */ "bvs", "adc", "nop", "rra", "nop", "adc", "ror", "rra", "sei", "adc", "nop", "rra", "nop", "adc", "ror", "rra", /* 7 */
/* 8 */ "nop", "sta", "nop", "sax", "sty", "sta", "stx", "sax", "dey", "nop", "txa", "nop", "sty", "sta", "stx", "sax", /* 8 */
/* 9 */ "bcc", "sta", "nop", "nop", "sty", "sta", "stx", "sax", "tya", "sta", "txs", "nop", "nop", "sta", "nop", "nop", /* 9 */
/* A */ "ldy", "lda", "ldx", "lax", "ldy", "lda", "ldx", "lax", "tay", "lda", "tax", "nop", "ldy", "lda", "ldx", "lax", /* A */
/* B */ "bcs", "lda", "nop", "lax", "ldy", "lda", "ldx", "lax", "clv", "lda", "tsx", "lax", "ldy", "lda", "ldx", "lax", /* B */
/* C */ "cpy", "cmp", "nop", "dcp", "cpy", "cmp", "dec", "dcp", "iny", "cmp", "dex", "nop", "cpy", "cmp", "dec", "dcp", /* C */
/* D */ "bne", "cmp", "nop", "dcp", "nop", "cmp", "dec", "dcp", "cld", "cmp", "nop", "dcp", "nop", "cmp", "dec", "dcp", /* D */
/* E */ "cpx", "sbc", "nop", "isb", "cpx", "sbc", "inc", "isb", "inx", "sbc", "nop", "sbc", "cpx", "sbc", "inc", "isb", /* E */
/* F */ "beq", "sbc", "nop", "isb", "nop", "sbc", "inc", "isb", "sed", "sbc", "nop", "isb", "nop", "sbc", "inc", "isb"  /* F */
};

diassembleResult_t dissassemble(uint8_t *ram) {

  static char assembler[STR_SIZE] = "";
  static char *pattern = "%s %s";
  const addresstableResult_t atr = addrtable[ram[0]](&ram[1]);
  snprintf(assembler, STR_SIZE, pattern, opcodes[ram[0]], atr.address);
  return (diassembleResult_t){assembler, atr.length + 1, ram};
}

// This holds detailed information about ste state of the CPU for each memory access.
typedef struct {
    uint16_t address;
    uint8_t value;
    uint16_t pc;
    uint8_t sp, a, x, y, status;
    enum {begin = 0, read = 1, write = 2, end = 3} type;
} accessState_t;

static struct {  // Structure of all state (including memory reads and writes) during "step".
    accessState_t accesses[16]; // 10 should be enough but just making sure!
    size_t count;
} stepInfo;

void pushMemoryAccess(accessState_t state) {
    if (stepInfo.count < sizeof(stepInfo.accesses) / sizeof(accessState_t)) {
        stepInfo.accesses[stepInfo.count] = state;
        ++stepInfo.count;
    }
}
void instructionBegin() {
    memset(&stepInfo, 0, sizeof(stepInfo));
    accessState_t state = {0x0, 0x0, pc, sp, a, x, y, status, begin};
    pushMemoryAccess(state);
}

void memoryRead(uint16_t address, uint8_t value) {
    accessState_t state = {address, value, pc, sp, a, x, y, status, read};
    pushMemoryAccess(state);
}

void memoryWrite(uint16_t address, uint8_t value){
    accessState_t state = {address, value, pc, sp, a, x, y, status, write};
    pushMemoryAccess(state);
}

void instructionCompleted(){
    accessState_t state = {0x0, 0x0, pc, sp, a, x, y, status, end};
    pushMemoryAccess(state);
}

void printInstructionTraceInfo(FILE *fp) {
    fprintf(fp, "STATE .. : Step   Address   Data    PC       A      X      Y       SP    NV-BDIZC\n");
    for(size_t i = 0; i < stepInfo.count; ++i) {
        accessState_t *state = &stepInfo.accesses[i];
        char const *types[] = {"Begin", "Read ", "Write", "End  "};
        fprintf(fp, "STATE .. : %s   $%04x    $%02x    $%04x    $%02x    $%02x    $%02x     $%02x    %d%d%d%d%d%d%d%d\n",
               types[state->type], state->address, state->value, state->pc, state->a, state->x, state->y, state->sp,
               (state->status & FLAG_SIGN) >> 7,
               (state->status & FLAG_OVERFLOW) >> 6,
               (state->status & FLAG_CONSTANT) >> 5,
               (state->status & FLAG_BREAK) >> 4,
               (state->status & FLAG_DECIMAL) >> 3,
               (state->status & FLAG_INTERRUPT) >> 2,
               (state->status & FLAG_ZERO) >> 1,
               (state->status & FLAG_CARRY));
    }
}