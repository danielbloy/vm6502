#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-signed-bitwise"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include "fake6502.h"
#include "input.h"
#include "output.h"
#include "dissassembler.h"
#include "watches.h"
#include "util.h"
#include "breakpoints.h"
#include "screen.h"

/**
 * Implements a very simple virtual machine with 64Kb RAM and a 6502 CPU.
 * The 6502 CPU is emulated using fake6502.c, developed by Mike Chambers
 * and available from at http://rubbermallet.org/fake6502.c.
 *
 * Input hex files can use the following commands:
 * #INFO
 * #VERBOSE
 * #NOFINALSTATE
 * #NOAUTOSTOP
 * #DUMPCPU
 * #DUMPWATCHES
 * #DUMPSCREEN
 *
 * STR: or STRING:
 * BREAK: [<address>]  Set a breakpoint either at address or current position.
 * WATCH: <watch>      Set a watch where watch is <address>[-<type>[-<length>]]
 * START: [<address>]  Set a start address
 * STOP: [<address>]   Set an auto stop address
 * ORG: <address>      Set position to begin loading next byte
 * HEX: <bytes>        Load some bytes at current address
 * <address>: <bytes>  Load some bytes at the given address
 *
 * <bytes>             A string a hexadecimal numbers separated by spaces. Can also be characters
 *                     in single quotes.
 *
 * Extending some ideas from http://www.6502asm.com, the CPU maps memory
 * addresses that allows the virtual machine to be run and interacted with
 * from a console command-line:
 *
 *  $FA - Read stdin (blocks) and returns first character; write char to stdout (non-blocking)
 *  $FB - Read next character from buffer or zero if none; write char to stderr (non-blocking)
 *  $FC - Read for random number; TODO Write reserved for blocking until pulse passed (60 hz).
 *  $FD - Reserved for OS request
 *  $FE - Reserved for OS request
 *  $FF - Write for OS request, Read for OS request reset, read A for OS result
 *
 *  0: stdin; 1: stdout; 2: stderr
 *  $FD, $FE, $FF - Read, clears request ready for next os request.
 *  TODO $FF - Write $00 - Read stdin using fgets(); write Address of buffer ($LL, $HH) then max length $BB, result in A.
 *  $FF - Write $01 - write stdout using fputs(); write address of buffer ($LL, $HH), A not changed.
 *  $FF - Write $02 - write stderr using fputs(); write address of buffer ($LL, $HH), A not changed.
 *  $FF - Write $03 - Prints screen, A not changed.
 *  $FF - Write $04 - String to integer - address of buffer ($LL, $HH) zero terminated, address to write 32-bit signed number ($LL, $HH), A not changed
 *  $FF - Write $05 - 8-bit unsigned Integer to string  - address of buffer ($LL, $HH), size of buffer including null terminator, number, A not changed
 *  $FF - Write $06 - 8-bit signed Integer to string    - address of buffer ($LL, $HH), size of buffer including null terminator, number, A not changed
 *  $FF - Write $07 - 16-bit unsigned Integer to string - address of buffer ($LL, $HH), size of buffer including null terminator, number ($LL, $HH), A not changed
 *  $FF - Write $08 - 16-bit signed Integer to string   - address of buffer ($LL, $HH), size of buffer including null terminator, number ($LL, $HH), A not changed
 *  $FF - Write $09 - 24-bit unsigned Integer to string - address of buffer ($LL, $HH), size of buffer including null terminator, number ($LL, $MM, $HH), A not changed
 *  $FF - Write $0A - 24-bit signed Integer to string   - address of buffer ($LL, $HH), size of buffer including null terminator, number ($LL, $MM, $HH), A not changed
 *  $FF - Write $0B - 32-bit unsigned Integer to string - address of buffer ($LL, $HH), size of buffer including null terminator, number ($LL, $ML, $MH, $HH), A not changed
 *  $FF - Write $0C - 32-bit signed Integer to string   - address of buffer ($LL, $HH), size of buffer including null terminator, number ($LL, $ML, $MH, $HH), A not changed
 *  TODO $FF - Get date and time.
 *  $FF - Write $FE - Terminate program with failure, A not changed.
 *  $FF - Write $FF - Terminate program with success, A not changed.
 *
 * The command line argument requires the location of a binary image to load
 * and begin executing. By default it expects a dasm type 1 image which has
 * the first two bytes reserved for the address to load into the RESET
 * addresses $FFFC and $FFFD; see https://www.pagetable.com/?p=410.
 */
// TODO: Advanced dissassembler what outputs state information too: LDA $0200  ; $0200 eq $0A  A->$0A

#define VERBOSE (verbose || stepping)
#define INFO    (info || stepping)

// These options control the operation of the virtual machine.
static char const* dumpfile = NULL;            // File to output the entire machine state to.
bool dumpCpu = false;                          // Prints the CPU state out at the end of the program.
bool dumpScreen = false;                       // Prints the screen memory out at the end of the program.
bool dumpWatches = false;                      // Prints the watches out at the end of the program.
bool finalState = true;                        // Prints CPU state and watches at the end of the execution of the program.
bool info = false;                             // In info mode we output some information but not everything.
bool verbose = false;                          // In verbose mode we get output of the CPU and watches at each step.
int screenMode = 0;                            // Mode 0 = 32x32, Mode 1 = 64 x 16.
static int exitCode = EXIT_SUCCESS;            // The exit code of the program being executed.
static bool breakpoints = true;                // By default breakpoints are enabled but can be disabled at the command-line.
// see https://stackoverflow.com/questions/4217037/catch-ctrl-c-in-c
static volatile sig_atomic_t stepping = false; // Are we single stepping the CPU?
static volatile sig_atomic_t stepsLeft = -1;   // -1 means infinite but otherwise a value of 0 or more means keep going.
static size_t steps = 0;                       // The number of steps actually executed.
static size_t filesLoaded = 0;                 // The number of files loaded into the virtual machine.
static uint16_t loadAddress = 0x0600;          // The default address that the file will be loaded at.

static struct { // Allows the start address to be specified at the command-line.
    bool enabled;
    uint16_t address;
} startAddress = {false, 0x0000};

static struct { // If auto stop is enabled, it defaults to the memory address of the last byte read in.
    bool enabled;
    uint16_t address;
} autoStop = {true, 0x0000};

static struct {
    uint8_t ram[RAM_SIZE]; // The actual RAM for the virtual machine. We give it the full 64Kb.
    uint8_t overflow[2];   // An extra couple of bytes to allow for dissassembly at the ram boundaries.
} vmRam = {0};

// Reads from stdin in blocking and non-blocking way
char getStdIn(bool block) {
    static char buffer[256] = {0};
    static char *pos = buffer; // Next character
    if (*pos) {
        char result = *pos;
        ++pos;
        return result;
    }

    if (!block) return 0;
    pos = buffer;
    *pos = 0;
    if (!fgets(buffer, 256, stdin)) return 0;
    if (!*pos) return 0;

    char result = *pos;
    ++pos;
    return result;
}

void uintToString(uint16_t bufaddress, uint8_t bufsize, const uint8_t bytes[4]) {
    static char buffer[64] = "";
    sprintf(buffer, "%u", (bytes[3] << 24) + (bytes[2] << 16) + (bytes[1] << 8) + bytes[0]);
    strncpy((char *)&vmRam.ram[bufaddress], buffer, bufsize);
    vmRam.ram[bufaddress + bufsize - 1] = 0;
}

void intToString(uint16_t bufaddress, uint8_t bufsize, uint8_t bytes[4], size_t numbytes) {
    if (numbytes > 4) return;
    static char buffer[64] = "";
    static int64_t const maxNeg[] = {-128, -32768, -8388608, -2147483648};

    // Determine if negative number using the last bit of the last byte.
    bool negative = bytes[numbytes - 1] & 0x80;
    int64_t num = negative ? maxNeg[numbytes - 1] : 0;

    // Kill the sign.
    bytes[numbytes - 1] &= 0x7F;

    // Add positive components.
    num += (bytes[3] << 24) + (bytes[2] << 16) + (bytes[1] << 8) + bytes[0];
    sprintf(buffer, "%ld", num);
    strncpy((char *)&vmRam.ram[bufaddress], buffer, bufsize);
    vmRam.ram[bufaddress + bufsize - 1] = 0;
}

void stringToInt(uint16_t strAddress, uint16_t intAddress) {
    int num = atoi((char const *)&vmRam.ram[strAddress]); // NOLINT(cert-err34-c)
    // NOTE: This assumes that the implementation of signed numbers in the host machine is 2's complement.
    vmRam.ram[intAddress]     = num & 0xFF;
    vmRam.ram[intAddress + 1] = (num >> 8) & 0xFF;
    vmRam.ram[intAddress + 2] = (num >> 16) & 0xFF;
    vmRam.ram[intAddress + 3] = (num >> 24) & 0xFF;
}

#define OS_RESET  -1
#define OS_REQUEST 1

#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-multiway-paths-covered"
uint8_t osRequest(uint16_t address, int operation, uint8_t value) {

    static uint8_t operation0xfdBuf[5] = {0};
    static uint8_t operation0xfeBuf[5] = {0};
    static uint8_t operation0xffBuf[5] = {0};

    static int operation0xfdCount = 0;
    static int operation0xfeCount = 0;
    static int operation0xffCount = 0;

    // Pointers to the correct static state data.
    uint8_t *operationBuf   = address == 0xfd ? operation0xfdBuf    : address == 0xfe ? operation0xfeBuf    : operation0xffBuf;
    int     *operationCount = address == 0xfd ? &operation0xfdCount : address == 0xfe ? &operation0xfeCount : &operation0xffCount;

    // If a reset then simply exit.
    if (operation == OS_RESET) { *operationCount = 0; return 0; }

    // Put the item in the buffer
    operationBuf[*operationCount] = value;
    ++(*operationCount);

    if (address != 0xff) return 0;
    uint8_t result = 0;

    // Is it a single byte request?
    if (*operationCount == 1) {
        switch (value) {
            case 0x03: // $03 - Prints screen, A not changed.
            case 0xfe: // $FE - Terminate program with failure, A not changed.
            case 0xff: // $FF - Terminate program with success, A not changed.
                if (value == 0x03) { printScreen(stdout, vmRam.ram, false); }
                if (value == 0xfe) { exitCode = EXIT_FAILURE; stepsLeft = 0; } // Terminate program with failure
                if (value == 0xff) { exitCode = EXIT_SUCCESS; stepsLeft = 0; } // Terminate program with success
                (*operationCount) = 0; // Implicit reset.
        }
        return result;
    }

    if (*operationCount == 2) {
        return result;
    }

    // Bytes 2 and 3 are likely to be an address.
    uint16_t addr = (operationBuf[2] << 8) + operationBuf[1];

    uint8_t bytes[4] = {0};

    // Is it a triple byte request?
    if (*operationCount == 3) {
        switch (operationBuf[0]) {
            case 0x00:
            case 0x01: // $01 - write stdout using fputs(); write address of buffer ($LL, $HH), A not changed.
            case 0x02: // $02 - write stderr using fputs(); write address of buffer ($LL, $HH), A not changed.
                if (operationBuf[0] == 0x01) { fprintf(stdout, "%s", &(vmRam.ram[addr])); }
                if (operationBuf[0] == 0x02) { fprintf(stderr, "%s", &(vmRam.ram[addr])); }
                (*operationCount) = 0; // Implicit reset.
        }
        return result;
    }

    if (*operationCount == 4) {
        return result;
    }

    // Bytes 4 and 5 are likely to be an address.
    uint16_t addr2 = (operationBuf[4] << 8) + operationBuf[3];

    if (*operationCount == 5) {
        switch (operationBuf[0]) {
            case 0x04: // $FF - Write $04 - String to integer - address of buffer ($LL, $HH) zero terminated, address to write 32-bit signed number ($LL, $HH), A not changed
            case 0x05: // $05 - 8-bit unsigned Integer to string - address of buffer ($LL, $HH), size of buffer including null terminator, number, A not changed
            case 0x06: // $06 - 8-bit signed Integer to string   - address of buffer ($LL, $HH), size of buffer including null terminator, number, A not changed
                bytes[0] = operationBuf[4];
                if (operationBuf[0] == 0x04) stringToInt(addr, addr2);
                if (operationBuf[0] == 0x05 && operationBuf[3] > 0) uintToString(addr, operationBuf[3], bytes);
                if (operationBuf[0] == 0x06 && operationBuf[3] > 0) intToString(addr, operationBuf[3], bytes, 1);
                (*operationCount) = 0; // Implicit reset.
        }
        return result;
    }

    if (*operationCount == 6) {
        switch (operationBuf[0]) {
            case 0x07: // $07 - 16-bit unsigned Integer to string - address of buffer ($LL, $HH), size of buffer including null terminator, number ($LL, $HH), A not changed
            case 0x08: // $08 - 16-bit signed Integer to string   - address of buffer ($LL, $HH), size of buffer including null terminator, number ($LL, $HH), A not changed
                bytes[0] = operationBuf[4];
                bytes[1] = operationBuf[5];
                if (operationBuf[0] == 0x07 && operationBuf[3] > 0) uintToString(addr, operationBuf[3], bytes);
                if (operationBuf[0] == 0x08 && operationBuf[3] > 0) intToString(addr, operationBuf[3], bytes, 2);
                (*operationCount) = 0; // Implicit reset.
        }
        return result;
    }

    if (*operationCount == 7) {
        switch (operationBuf[0]) {
            case 0x09: // $09 - 24-bit unsigned Integer to string - address of buffer ($LL, $HH), size of buffer including null terminator, number ($LL, $MM, $HH), A not changed
            case 0x0A: // $0A - 24-bit signed Integer to string   - address of buffer ($LL, $HH), size of buffer including null terminator, number ($LL, $MM, $HH), A not changed
                bytes[0] = operationBuf[4];
                bytes[1] = operationBuf[5];
                bytes[2] = operationBuf[6];
                if (operationBuf[0] == 0x09 && operationBuf[3] > 0) uintToString(addr, operationBuf[3], bytes);
                if (operationBuf[0] == 0x0A && operationBuf[3] > 0) intToString(addr, operationBuf[3], bytes, 3);
                (*operationCount) = 0; // Implicit reset.
        }
        return result;
    }

    if (*operationCount == 8) {
        switch (operationBuf[0]) {
            case 0x0B: // $0B - 32-bit unsigned Integer to string - address of buffer ($LL, $HH), size of buffer including null terminator, number ($LL, $ML, $MH, $HH), A not changed
            case 0x0C: // $0C - 32-bit signed Integer to string   - address of buffer ($LL, $HH), size of buffer including null terminator, number ($LL, $ML, $MH, $HH), A not changed
                bytes[0] = operationBuf[4];
                bytes[1] = operationBuf[5];
                bytes[2] = operationBuf[6];
                bytes[3] = operationBuf[7];
                if (operationBuf[0] == 0x0B && operationBuf[3] > 0) uintToString(addr, operationBuf[3], bytes);
                if (operationBuf[0] == 0x0C && operationBuf[3] > 0) intToString(addr, operationBuf[3], bytes, 4);
                (*operationCount) = 0; // Implicit reset.
        }
        return result;
    }

    return result;
}
#pragma clang diagnostic pop

uint8_t read6502(uint16_t address) {
    if (VERBOSE) memoryRead(address, vmRam.ram[address]);
    if (autoStop.enabled && address == autoStop.address) stepsLeft = 0;
    if (!stepping && breakpoints && hasHitBreakpoint(address)) {
        stepping = true;
        puts("---------------------------------------------------------------------------------");
        printf("INFO ... : Hit breakpoint set at $%04x.\n", address);
    }
    if (address == 0xfa) return getStdIn(true);
    if (address == 0xfb) return getStdIn(false);
    if (address == 0xfc) {
        static bool first = true;
        if (first) {
            time_t t;
            // Intialise random number generator
            srand((unsigned) time(&t)); // NOLINT(cert-msc51-cpp)
            first = false;
        }
        return rand() % 255; // NOLINT(cert-msc50-cpp)
    }
    if (address == 0xfd || address == 0xfe || address == 0xff ) return osRequest(address, OS_RESET, 0);

    return vmRam.ram[address];
}

void write6502(uint16_t address, uint8_t value) {
    if (VERBOSE) memoryWrite(address, value);
    vmRam.ram[address] = value;
    if (value && address == 0xfa) fputc(value == 0x10 || value == 0x13 ? '\n' : value, stdout);
    if (value && address == 0xfb) fputc(value == 0x10 || value == 0x13 ? '\n' : value, stderr);
    if (address == 0xfd || address == 0xfe || address == 0xff ) { osRequest(address, OS_REQUEST, value); return; }
}

// Converts the hex specified string from the parameter into a number and returns it.
// Exits the process if the value is not a valid hex number.
static uint16_t getAddressFromParameter(char const *parameter) {
    char const *number = strchr(parameter, '=');
    ++number;
    if (!isHexNumber(number)) {
        printf("ERROR: The value '%s' is not a valid hexadecimal number!", number);
        exit(1);
    }
    return (uint16_t)strtol(number, NULL, 16);
}

// Converts the string from the parameter into a number and returns it.
// Exits the process if the value is not a valid hex number.
static uint16_t getNumberFromParameter(char const *parameter) {
    char const *number = strchr(parameter, '=');
    ++number;
    if (!isDecimalNumber(number)) {
        printf("ERROR: The value '%s' is not a valid decimal number!", number);
        exit(1);
    }
    return (uint16_t)strtol(number, NULL, 0);
}

// Returns the start of the string after the = in the parameter.
// NOTE: This returns the bare pointer to the string.
static char const *getStringFromParameter(char const *parameter) {
    char const *string = strchr(parameter, '=');
    ++string;
    return string;
}

// Returns the start of the string after the = in the parameter.
// NOTE: This returns the bare pointer to the string.
static char const *getFilenameFromParameter(char const *parameter) {
    char const *filename = getStringFromParameter(parameter);

    // If no filename was specified then error.
    if (!filename || !strlen(filename)) {
        puts("ERROR: No filename was specified!");
        exit(1);
    }

    return filename;
}

// Holds the surrounding logic required for loading a file such as checking the file exists.
// NOTE: The actual loading is done by the load function passed in.
static void loadFile(char const *arg, loadFile_t(*loadfunc)(FILE *, uint8_t *, uint16_t)) {
    char const *filename = getFilenameFromParameter(arg);

    // Validate that the file exists.
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("ERROR: There was a problem opening the file '%s'!\n", filename);
        exit(1);
    }
    // This returns the startAddress (defaults to loadAddress) and the stopAddress (defaults to last byte read in).
    loadFile_t lf = loadfunc(fp, vmRam.ram, loadAddress);
    ++filesLoaded;

    fclose(fp);

    if (lf.disableAutoStop) autoStop.enabled = false;

    // Setup default start and stop addresses; these can be overridden at the command-line.
    vmRam.ram[0xfffc] = LOW_BYTE(lf.startAddress);
    vmRam.ram[0xfffd] = HIGH_BYTE(lf.startAddress);
    autoStop.address = lf.stopAddress;
}

static void showCPU() {
    puts("---------------------------------------------------------------------------------");
    printCpu(stdout);
    puts("---------------------------------------------------------------------------------");
}

static void showBreakpoints() {
    puts("---------------------------------------------------------------------------------");
    listBreakpoints(stdout, vmRam.ram);
    puts("---------------------------------------------------------------------------------");
}

static void showWatches() {
    puts("---------------------------------------------------------------------------------");
    listWatches(stdout, vmRam.ram);
    puts("---------------------------------------------------------------------------------");
}

// Gets a hex value from the user and passes it to the callback.
static bool getAndUseHexNumber(char const * msg, void(*callback)(uint16_t)) {
    char buffer[256];
    printf("%s ", msg);
    if (!fgets(buffer, sizeof(buffer)/sizeof(char), stdin)) return false;
    if (isEmpty(buffer)) return false;
    if (!strlen(buffer)) return false;
    if (!isHexNumber(buffer)) {
        printf("ERROR: The value '%s' is not a valid hex number!\n", buffer);
        return false;
    }

    callback((uint16_t)strtol(buffer, NULL, 16));
    return true;
}

static bool getAndUseHexNumberForCPU(char const * msg, void(*callback)(uint16_t)) {
    if (!getAndUseHexNumber(msg, callback)) return false;
    showCPU();
    return true;
}

uint16_t pokeAddress = 0;
static void pokeValue(uint16_t value) {
    vmRam.ram[pokeAddress] = value;
}
static void pokeValues(uint16_t address) {
    do {
        pokeAddress = address;
        ++address; // Advance for next address.
        printf("Enter value to poke into address $%04x or just ENTER to finish:", pokeAddress);
    } while (getAndUseHexNumber("", pokeValue));
    showWatches();
}

static void setProgramCounter(uint16_t value) {
    pc = value;
}

static void setStackPointer(uint16_t value) {
    sp = (uint8_t)value;
}

static void setRegisterA(uint16_t value) {
    a = (uint8_t)value;
}

static void setRegisterX(uint16_t value) {
    x = (uint8_t)value;
}

static void setRegisterY(uint16_t value) {
    y = (uint8_t)value;
}

static void setStatusRegister(uint16_t value) {
    status = (uint8_t)value;
}

static void setBreakpoint(uint16_t address) {
    // As this is used from the menu only, setting a breakpoint will enable breakpoints
    // even if they were previously disabled.
    breakpoints = true;
    if (addBreakpoint(address)) showBreakpoints();
}

static void deleteBreakpoint(uint16_t address) {
    if (removeBreakpoint(address)) showBreakpoints();
}

static void deleteWatch(uint16_t watch) {
    if (removeWatch(watch)) showWatches();
}

static void setAutoStopAddress(uint16_t address) {
    autoStop.enabled = true;
    autoStop.address = address;
}

static void setStartAddress(uint16_t address) {
    startAddress.enabled = true;
    startAddress.address = address;
}

// Signal handler that puts the application into stepping mode.
static void handler(int sig) {
    signal(sig, SIG_IGN);
    stepping = true;
    puts("---------------------------------------------------------------------------------");
    printf("INFO ... : Program interrupted at address $%04x.\n", pc);
    signal(SIGINT, handler);
}

static void showMenu() {
    puts("MENU ... :");
    puts("MENU ... :   ---- B R E A K P O I N T S ----");
    puts("MENU ... :   b to set a breakpoint");
    puts("MENU ... :   -b to remove a breakpoint");
    puts("MENU ... :   lb to list breakpoints");
    puts("MENU ... :   nobreak to disable breakpoints");
    puts("MENU ... :");
    puts("MENU ... :   ---- W A T C H E S ----");
    puts("MENU ... :   w to add a watch");
    puts("MENU ... :   -w to remove a watch");
    puts("MENU ... :   lw to list watches");
    puts("MENU ... :");
    puts("MENU ... :   ---- C P U ----");
    puts("MENU ... :   cpu to show the CPU");
    puts("MENU ... :   pc to set program counter");
    puts("MENU ... :   sp to set stack pointer");
    puts("MENU ... :   A to set A register");
    puts("MENU ... :   X to set X register");
    puts("MENU ... :   Y to set Y register");
    puts("MENU ... :   S to set Status register");
    puts("MENU ... :   stop to set auto stop address");
    puts("MENU ... :");
    puts("MENU ... :   ---- M E M O R Y ----");
    puts("MENU ... :   poke to poke a value into ram");
    puts("MENU ... :   screen to output the screen");
    puts("MENU ... :");
    puts("MENU ... :   ---- E X E C U T I O N ----");
    puts("MENU ... :   q, quit, exit to quit");
    puts("MENU ... :   r to run");
    puts("MENU ... :   stop to set auto stop address");
    puts("MENU ... :   ENTER to step");
    puts("MENU ... :");
}

int main(int argc, char *argv[]) {
    if (argc <= 1) printHelpAndExit();
    for (int i = 1; i < argc; ++i) {
        char const *arg = argv[i];

        // Help and version information.
        if (strcasecmp("-h",        arg) == 0) printHelpAndExit();
        if (strcasecmp("--h",       arg) == 0) printHelpAndExit();
        if (strcasecmp("-help",     arg) == 0) printHelpAndExit();
        if (strcasecmp("--help",    arg) == 0) printHelpAndExit();
        if (strcasecmp("-v",        arg) == 0) printVersionAndExit();
        if (strcasecmp("--v",       arg) == 0) printVersionAndExit();
        if (strcasecmp("-version",  arg) == 0) printVersionAndExit();
        if (strcasecmp("--version", arg) == 0) printVersionAndExit();

        // Simple switches.
        if (strcasecmp("--noautostop",    arg) == 0) { autoStop.enabled = false; continue; }
        if (strcasecmp("--nobreakpoints", arg) == 0) { breakpoints = false; continue; }
        if (strcasecmp("--nofinalstate",  arg) == 0) { finalState = false; continue; }
        if (strcasecmp("--info",          arg) == 0) { info = true; continue; }
        if (strcasecmp("--verbose",       arg) == 0) { info = true; verbose = true; continue; }
        if (strcasecmp("--step",          arg) == 0) { stepping = true; continue; }
        if (strcasecmp("--dumpscreen",    arg) == 0) { dumpScreen = true; continue; }
        if (strcasecmp("--dumpwatches",   arg) == 0) { dumpWatches = true; continue; }
        if (strcasecmp("--screenmode1",   arg) == 0) { screenMode = 1; continue; }

        // Hex value switches.
        if (strncasecmp("--maxsteps=",     arg, 11) == 0) { stepsLeft = getNumberFromParameter(arg); continue; }
        if (strncasecmp("--loadaddress=",  arg, 14) == 0) { loadAddress = getAddressFromParameter(arg); continue; }
        if (strncasecmp("--startaddress=", arg, 15) == 0) { setStartAddress(getAddressFromParameter(arg)); continue; }
        if (strncasecmp("--stopaddress=",  arg, 14) == 0) { setAutoStopAddress(getAddressFromParameter(arg)); continue; }

        // Load a file.
        if (strncasecmp("--binfile=", arg, 10) == 0) { loadFile(arg, loadBinFile); continue; }
        if (strncasecmp("--hexfile=", arg, 10) == 0) { loadFile(arg, loadHexFile); continue; }

        // Dumping the file out.
        if (strncasecmp("--dumpfile=", arg, 11) == 0) { dumpfile = getFilenameFromParameter(arg); continue; }

        // Adding a watch or breakpoint.
        if (strncasecmp("--watch=", arg, 8) == 0) {
            if (!addWatchString(getStringFromParameter(arg))) exit(1);
            continue;
        }
        if (strncasecmp("--breakpoint=", arg, 13) == 0) {
            if (!addBreakpoint(getAddressFromParameter(arg))) exit(1);
            continue;
        }

        // Unknown argument so error.
        printf("ERROR: The argument '%s' is unknown!\n", arg);
        exit(1);
    }

    if (!filesLoaded) {
        puts("ERROR: No file was specified, please use --binfile or --hexfile!");
        exit(1);
    }

    signal(SIGINT, handler);

    // Enable the manual override of the start address; this overrides even hex files.
    if (startAddress.enabled) {
        vmRam.ram[0xfffc] = LOW_BYTE(startAddress.address);
        vmRam.ram[0xfffd] = HIGH_BYTE(startAddress.address);
    }

    if (INFO) {
        puts("---------------------------------------------------------------------------------");
        if (startAddress.enabled)
            printf("INFO ... : Start address ...... : $%04x\n", startAddress.address);
        else
            printf("INFO ... : Start address ...... : $%02x%02x\n", vmRam.ram[0xfffd], vmRam.ram[0xfffc]);

        if (autoStop.enabled)
            printf("INFO ... : Auto stop address .. : $%04x\n", autoStop.address);
        else
            puts("INFO ... : Auto stop disabled");
    }

    // Execute.
    char buffer[256];
    if (info | stepping) puts("INFO ... : CPU reset.");
    reset6502();
    if (VERBOSE) printCpuWatchesAndBreakpoints(stdout, vmRam.ram);
    do {
        if (stepping) {
            while(true) {
                printf("MENU ... : Paused, enter help for the menu or just ENTER to step: ");
                char key = *fgets(buffer, sizeof(buffer)/sizeof(char), stdin);
                if (key == 0) break;
                if (isEmpty(buffer)) break;
                if (strncasecmp("quit", buffer, 4) == 0) {stepsLeft = 0; break;}
                if (strncasecmp("exit", buffer, 4) == 0) {stepsLeft = 0; break;}
                if (key == 'q' || key == 'Q') {stepsLeft = 0; break;}
                if (key == 'r' || key == 'R') {stepping = false; break;}

                if (key == 'm' || key == 'M') {showMenu(); continue;}
                if (key == 'h' || key == 'H') {showMenu(); continue;}
                if (strncasecmp("menu", buffer, 4) == 0) {showMenu(); continue;}
                if (strncasecmp("help", buffer, 4) == 0) {showMenu(); continue;}

                if (strncasecmp("poke", buffer, 4) == 0) {getAndUseHexNumber("MENU ... : Enter address to start poke:", pokeValues); continue;}
                if (strncasecmp("stop", buffer, 4) == 0) {getAndUseHexNumber("MENU ... : Enter address to auto stop at:", setAutoStopAddress); continue;}
                if (strncasecmp("screen", buffer, 6) == 0) {printScreen(stdout, vmRam.ram, true); continue;}

                if (strncasecmp("cpu", buffer, 3) == 0) {showCPU(); continue;}
                if (strncasecmp("pc", buffer, 2) == 0) {getAndUseHexNumberForCPU("MENU ... : Enter new address for the program counter:", setProgramCounter); continue;}
                if (strncasecmp("sp", buffer, 2) == 0) {getAndUseHexNumberForCPU("MENU ... : Enter new address for the stack register:", setStackPointer); continue;}
                if (key == 'a' || key == 'A') {getAndUseHexNumberForCPU("MENU ... : Enter new value for the A register:", setRegisterA); continue;}
                if (key == 'x' || key == 'X') {getAndUseHexNumberForCPU("MENU ... : Enter new value for the X register:", setRegisterX); continue;}
                if (key == 'y' || key == 'Y') {getAndUseHexNumberForCPU("MENU ... : Enter new value for the Y register:", setRegisterY); continue;}
                if (key == 's' || key == 'S') {getAndUseHexNumberForCPU("MENU ... : Enter new value for the Status register:", setStatusRegister); continue;}

                if (strncasecmp("nobreak", buffer, 7) == 0) {breakpoints = false; continue;}
                if (key == 'b' || key == 'B') {getAndUseHexNumber("MENU ... : Enter address for the breakpoint:", setBreakpoint); continue;}
                if (strncasecmp("-b", buffer, 2) == 0) {getAndUseHexNumber("MENU ... : Enter address of breakpoint to remove:", deleteBreakpoint); continue;}
                if (strncasecmp("lb", buffer, 2) == 0) {showBreakpoints(); continue;}

                if (strncasecmp("-w", buffer, 2) == 0) {getAndUseHexNumber("MENU ... : Enter watch number to remove:", deleteWatch); continue;}
                if (strncasecmp("lw", buffer, 2) == 0) {showWatches(); continue;}
                if (key == 'w' || key == 'W') {
                    printf("MENU ... : Enter watch of the form: <address>[-<type>[-<length>]] where type is\n");
                    printf("MENU ... : one of: H (Hex), A (Assembler), S (String), C (ASCII Characters): ");
                    if (fgets(buffer, sizeof(buffer)/sizeof(char), stdin)) {
                        if (addWatchString(buffer)) {
                            showWatches();
                        }
                    }
                }
            }
        }

        if (stepsLeft != 0) { // This supports --maxsteps=0
            if (INFO) printf("INFO ... : CPU execute: $%04x: %s\n", pc, dissassemble(&vmRam.ram[pc]).assembler);
            instructionBegin();
            step6502();
            ++steps;
            instructionCompleted();
            if (VERBOSE) {
                printInstructionTraceInfo(stdout);
                printCpuWatchesAndBreakpoints(stdout, vmRam.ram);
            }
        }
        if (stepsLeft > 0) --stepsLeft;
        if (stepsLeft == 0) break;
    } while (1);

    if (INFO) {
        puts("INFO ... : CPU stopped.");
    }
    if (INFO || finalState) printf("INFO ... : %zu steps completed.\n", steps);
    if (finalState) printCpuWatchesAndBreakpoints(stdout, vmRam.ram);
    else {
        if (dumpCpu)     printCpuLine(stdout);
        if (dumpWatches) printWatches(stdout, vmRam.ram);
    }
    if (dumpScreen) printScreen(stdout, vmRam.ram, true);
    if (dumpfile) {
        FILE *fp = fopen(dumpfile, "w");
        if (!fp) {
            printf("ERROR: There was a problem opening the file '%s'!\n", dumpfile);
            exit(1);
        }
        printRamAndCpu(fp, vmRam.ram);
        if (dumpScreen) printScreen(fp, vmRam.ram, true);
        fclose(fp);
    }

    return exitCode;
}