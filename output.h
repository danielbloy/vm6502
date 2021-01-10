#pragma once

#include <stdint.h>
#include <stdio.h>

void printHelpAndExit();
void printVersionAndExit();
void printCpu(FILE *fp);
void printCpuLine(FILE *fp);
void printWatches(FILE *fp, uint8_t *ram);
void printCpuWatchesAndBreakpoints(FILE *fp, uint8_t *ram);
void printRamAndCpu(FILE *fp, uint8_t *ram);
