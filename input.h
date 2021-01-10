#pragma once

#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint16_t startAddress;
    uint16_t stopAddress;
    bool disableAutoStop; // If true autostop will be disable. If false, autostop will remain as it was set.
} loadFile_t;

loadFile_t loadBinFile(FILE *, uint8_t *ram, uint16_t loadAddress);
loadFile_t loadHexFile(FILE *, uint8_t *ram, uint16_t loadAddress);
