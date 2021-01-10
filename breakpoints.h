#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_BREAKPOINTS 64

size_t countBreakpoints();
size_t addBreakpoint(uint16_t address);
size_t removeBreakpoint(uint16_t address);
void listBreakpoints(FILE *fp, uint8_t *ram);
bool hasHitBreakpoint(uint16_t address);
