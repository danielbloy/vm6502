#pragma once

#include <stdbool.h>

bool dumpCpu;     // Prints the CPU state out at the end of the program.
bool dumpScreen;  // Prints the screen memory out at the end of the program.
bool dumpWatches; // Prints the watches out at the end of the program.
bool finalState;  // Prints CPU state and watches at the end of the execution of the program.
bool info;        // In info mode we output some information but not everything.
bool verbose;     // In verbose mode we get output of the CPU and watches at each step.
int screenMode;   // Mode 0 = 32x32, Mode 1 = 64 x 16.

bool isHexNumber(char const * str);
bool isDecimalNumber(char const * str);
bool isEmpty(char const *str);
