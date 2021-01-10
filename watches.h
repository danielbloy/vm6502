#pragma once

#include <stdint.h>
#include <stdio.h>

#define MAX_WATCHES 64

typedef struct {
    uint16_t address;
    uint16_t length;
    enum {hex = 0, ascii, string, assembler} type;
} watch_t;

size_t countWatches();
size_t addWatch(watch_t watch);
size_t addWatchString(char const *str); // Add a watch in string form of <address><type><length>
size_t removeWatch(size_t num);
void listWatches(FILE *fp, uint8_t *ram);
