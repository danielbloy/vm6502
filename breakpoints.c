#include "breakpoints.h"
#include "dissassembler.h"

typedef struct {
    uint16_t address;
    bool enabled;
} breakpoint_t;


// Breakpoints need just an address.
static breakpoint_t breakpoints[MAX_BREAKPOINTS] = {0};
static size_t numBreakpoints = 0;

size_t countBreakpoints() {
    return numBreakpoints;
}

// If the address has already been added, then return the current breakpoint.
size_t addBreakpoint(uint16_t address) {
    if (numBreakpoints >= MAX_BREAKPOINTS) {
        printf("ERROR: Cannot add breakpoint as the maximum number of breakpoints (%d) has been reached!\n", MAX_BREAKPOINTS);
        return 0;
    }

    // Loop through and check that address has not already been added.
    for (size_t i = 0; i < MAX_BREAKPOINTS; ++i) {
        if (breakpoints[i].enabled && breakpoints[i].address == address) {
            printf("ERROR: A breakpoint at the address $%04x has already been set!\n", address);
            return i;
        }
    }

    for (size_t i = 0; i < MAX_BREAKPOINTS; ++i) {
        if (!breakpoints[i].enabled) {
            breakpoints[i].address = address;
            breakpoints[i].enabled = true;
            ++numBreakpoints;
            break;
        }
    }

    return numBreakpoints;
}

size_t removeBreakpoint(uint16_t address) {
    // Loop through and locate the breakpoint and reduce numBreakpoints.
    for (size_t i = 0; i < MAX_BREAKPOINTS; ++i) {
        if (breakpoints[i].enabled && breakpoints[i].address == address) {
            breakpoints[i].enabled = false;
            --numBreakpoints;
            return numBreakpoints;
        }
    }

    // If we got here we couldn't find a breakpoint to remove.
    printf("ERROR: Cannot find breakpoint at address $%0x4!\n", address);
    return 0;
}

void listBreakpoints(FILE *fp, uint8_t *ram) {
    int num = 0;
    for(int i = 0; i < MAX_BREAKPOINTS; ++i) {
        if (!breakpoints[i].enabled) continue;
        ++num;
        fprintf(fp, "BREAK %02x : $%04x: %s\n", num, breakpoints[i].address, dissassemble(&ram[breakpoints[i].address]).assembler);
    }
}

bool hasHitBreakpoint(uint16_t address) {
    if (!numBreakpoints) return false;
    int num = 0;
    for(int i = 0; i < MAX_BREAKPOINTS; ++i) {
        if (!breakpoints[i].enabled) continue;
        ++num;
        if (breakpoints[i].address == address) return true;
        if (num >= numBreakpoints) return false; // Early exit if we have checked all breakpoints.
    }
    return false;
}