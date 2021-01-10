#include "screen.h"
#include "util.h"

void printScreen(FILE *fp, uint8_t *ram, bool prefix) {
    uint16_t address = 0x200;
    fprintf(fp, "%s+--------------------------------", prefix ? "SCREEN . :        " : "");
    fprintf(fp, "%s\n", screenMode == 0 ? "+" : "--------------------------------+");
    for (size_t y = 0; y < (screenMode == 0 ? 32 : 16); ++y) {
        if (prefix) fprintf(fp, "SCREEN . : $%04x: |", address); else fprintf(fp, "|");
        for (size_t x = 0; x < (screenMode == 0 ? 32 : 64); ++x, ++address) {
            fputc(ram[address] == 0 ? ' ' : ram[address] < 32 ? '#' : ram[address] > 126 ? '@' : (char)ram[address], fp);
        }
        fprintf(fp, "|\n");
    }
    fprintf(fp, "%s+--------------------------------", prefix ? "SCREEN . :        " : "");
    fprintf(fp, "%s\n", screenMode == 0 ? "+" : "--------------------------------+");
}
