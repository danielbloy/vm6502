#pragma ide diagnostic ignored "hicpp-signed-bitwise"
#include "output.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fake6502.h"
#include "dissassembler.h"
#include "watches.h"
#include "breakpoints.h"

void printHelpAndExit() {
    printf("%s, v%s: A simple 6502 virtual machine.\n\n", PROJECT_NAME, PROJECT_VER);
    printf("Usage:\n");
    printf("  %s [options]\n", PROJECT_NAME);
    printf("\n");
    printf("A very simple 6502 based virtual machine to aid learning of the 6502 processor, including\n");
    printf("assembly coding. The 6502 processor is wrapped in a virtual machine that allows interaction\n");
    printf("with the host console via exposure of stdin, stdout and stderr through memory mapping. This\n");
    printf("virtual machine is expected to be used in conjunction with a 6502 assembler such as dasm\n");
    printf("(see https://dasm-assembler.github.io/) but by using a file that conforms the the hex file\n");
    printf("format, it is possible to create programs without an assembler.\n");
    printf("\n");
    printf("The 6502 processor is emulated using fake6502 by Mike Chambers and is available at\n");
    printf("http://rubbermallet.org/fake6502.c.\n");
    printf("\n");
    printf("By default the virtual machine will auto stop after a read from a stop address. If the read is\n");
    printf("to get the instruction to execute or an operand then the instruction is still executed. By \n");
    printf("default, the stop address is the address of the last byte read in from the last file read. It\n");
    printf("can however be overridden by specifying it in a hex file or using the --stopaddress argument.\n");
    printf("\n");
    printf("By default the virtual machine will start execution from the load address of the last file read\n");
    printf("but this can be overridden by the --startaddress argument.\n");
    printf("\n");
    printf("The default load address is 600 hex but can be overridden in a hex file or by the --loadaddress\n");
    printf("argument. This argument can be specified multiple times and the value value that is used for a\n");
    printf("file is the last --loadaddress specified before the --binfile or --hexfile.\n");
    printf("\n");
    printf("Use either --binfile or --hexfile to specify a file to load. At least one file needs specifying\n");
    printf("but as many as required can be specified. The files are loaded in order and the default load\n");
    printf("address that applies is the last one specified before the file.\n");
    printf("\n");
    printf("The virtual machine offers a 'virtual display' of 32x32 (mode 0) or 64x16 characters (mode 1).\n");
    printf("This is not a display in the more traditional sense as it will only be output to the console on\n");
    printf("demand. The 'virtual display' begins at hex address $0200 and is exactly 1,024 bytes. The \n");
    printf("characters for the display are represented as single byte values in the range of 32 to 126.\n");
    printf("Values of less than 32 are displayed as the '#' character and values of greater than 126 are\n");
    printf("displayed as the '@' character.\n");
    printf("\n");
    printf("Options:\n");
    printf("-h, -H, --h, --H, -help, --help   Display this help.\n");
    printf("--binfile=<file>                  A 6502 binary file (with no origin header) to load.\n");
    printf("--breakpoint=<address>            Sets a breakpoint that triggers AFTER a read from the given\n");
    printf("                                  memory address. Once a breakpoint is hit, it places the CPU\n");
    printf("                                  into stepping mode.\n");
    printf("--dumpfile=<file>                 A file to dump the entire RAM and CPU state at the end of\n");
    printf("                                  program execution.\n");
    printf("--dumpcpu                         Outputs the CPU status to the console as a single line at the\n");
    printf("                                  end of program execution (regardless of whether --nofinalstate\n");
    printf("                                  was used or not.\n");
    printf("--dumpscreen                      Outputs the screen ram to the console at the end of program\n");
    printf("                                  execution.\n");
    printf("--dumpwatches                     Outputs the watches to the console at the end of the program\n");
    printf("                                  execution (regardless of whether --nofinalstate was used or not.\n");
    printf("--hexfile=<file>                  A 6502 hex file to load.\n");
    printf("--loadaddress=<address>           The address (in hex) to begin loading the file at (defaults\n");
    printf("                                  to 2000 hex).\n");
    printf("--info                            Output some debug information but not as much as verbose.\n");
    printf("--maxsteps=<number>               The maximum number of steps (instructions) to execute. Each\n");
    printf("                                  instruction may require reading more than one byte so the\n");
    printf("                                  number of memory reads will be more than the number of steps.\n");
    printf("--noautostop                      Turns off the auto stop functionality which terminates the\n");
    printf("                                  virtual machine after a read from the stop address memory\n");
    printf("                                  location. If the read is to get the instruction to execute\n");
    printf("                                  then the instruction is still executed.\n");
    printf("--nobreakpoints                   Disables all breakpoints. Breakpoints can be re-enabled by\n");
    printf("                                  setting a breakpoint at the execution menu (using CTRL-C when\n");
    printf("                                  the program is running).\n");
    printf("--nofinalstate                    Turns off the output of CPU information, watches and breakpoints\n");
    printf("                                  once the virtual machine has completed.\n");
    printf("--step                            Executes the virtual machine in single stepping mode which\n");
    printf("                                  requires the Enter key to be pressed after each instruction\n");
    printf("                                  is executed. This option also turns on --verbose.\n");
    printf("--startaddress=<address>          The start address (in hex) to begin execution from.\n");
    printf("--stopaddress=<address>           The stop address (in hex) to terminate execution at if auto\n");
    printf("                                  stop is enabled.\n");
    printf("--screenmode1                     Sets the screen mode to 64x16 mode rather than 32x32 mode.\n");
    printf("-v, -V, --v, --V, --version       Display version information.\n");
    printf("--verbose                         Provides verbose logging information which includes output\n");
    printf("                                  of all memory reads and writes. CPU information and watches\n");
    printf("                                  are output after each instruction is executed.\n");
    printf("--watch=<address>-<type>-<length> Specify a watch starting at the specified hex address, of\n");
    printf("                                  type H (Hex), A (Assembler), S (String), C (ASCII Characters)\n");
    printf("                                  and a length in hex. For example a 20 hex assembler watch\n");
    printf("                                  starting at 600 hex would be specified as --watch=600-A-20.\n");
    printf("                                  The length is optional and defaults to 10 hex except for\n");
    printf("                                  strings which default to 40 hex.\n");
    printf("\n");
    printf("Example 1 - Run an example hex file in stepping mode:\n");
    printf("    %s --hexfile=./examples/hexfiles/exmaple-6.hex --step\n", PROJECT_NAME);
    exit(0);
}

void printVersionAndExit() {
    printf("%s, v%s: A simple 6502 virtual machine.\n", PROJECT_NAME, PROJECT_VER);
    exit(0);
}


void printCpuLine(FILE *fp) {
    fprintf(fp, "CPU .... : PC $%04x      A $%02x      X $%02x      Y $%02x      SP $%02x       S %d%d%d%d%d%d%d%d\n", pc, a, x, y, sp,
            (status & FLAG_SIGN) >> 7,
            (status & FLAG_OVERFLOW) >> 6,
            (status & FLAG_CONSTANT) >> 5,
            (status & FLAG_BREAK) >> 4,
            (status & FLAG_DECIMAL) >> 3,
            (status & FLAG_INTERRUPT) >> 2,
            (status & FLAG_ZERO) >> 1,
            (status & FLAG_CARRY));
}

// Prints out the internal status of the CPU.
void printCpu(FILE *fp) {
    struct {
        const char * const set;
        const char * const unset;
        uint8_t flag;
    } flags[] = {
            {"NEGATIVE", "positive", FLAG_SIGN},
            {"OVERFLOW", "overflow", FLAG_OVERFLOW},
            {"-", "-", FLAG_CONSTANT},
            {"BREAK", "break", FLAG_BREAK},
            {"DECIMAL", " binary", FLAG_DECIMAL},
            {"INTERRUPT DISABLED", "interrupt  enabled", FLAG_INTERRUPT},
            {"ZERO", "zero", FLAG_ZERO},
            {"CARRY", "carry", FLAG_CARRY}
    };
    fprintf(fp, "CPU .... :");
    for (size_t i = 0, count = (sizeof flags / sizeof flags[0]); i < count; ++i) {
        if (i) fprintf(fp, ",");
        fprintf(fp, " %s", status & flags[i].flag ? flags[i].set : flags[i].unset);
    }
    fprintf(fp, "\n");
    fprintf(fp, "CPU .... :                                                               NV-BDIZC\n");
    printCpuLine(fp);
}

void printWatches(FILE *fp, uint8_t *ram) {
    if (countWatches()) {
        listWatches(fp, ram);
    }
}

void printCpuWatchesAndBreakpoints(FILE *fp, uint8_t *ram) {
    fputs("---------------------------------------------------------------------------------\n", fp);
    printCpu(fp);
    fputs("---------------------------------------------------------------------------------\n", fp);
    if (countWatches()) {
        listWatches(fp, ram);
        fputs("---------------------------------------------------------------------------------\n", fp);
    }
    if (countBreakpoints()) {
        listBreakpoints(fp, ram);
        fputs("---------------------------------------------------------------------------------\n", fp);
    }
}

// Prints RAM out in 32 byte chunks.
void printRamAndCpu(FILE *fp, uint8_t *ram) {
    uint16_t address = 0;

    do {
        if (address % 0x20 == 0) {
            if (address) fprintf(fp, "\n"); // We need to put a new line on all but the first line.
            fprintf(fp, "%04x: ", address);
        }
        fprintf(fp, "%02x ", ram[address]);
        ++address;
    } while (address); // We stop when we overflow and wrap back around to zero (as we use the max address)!.
    fprintf(fp, "\n");
    fputs("---------------------------------------------------------------------------------\n", fp);
    printCpu(fp);
    fputs("---------------------------------------------------------------------------------\n", fp);
}