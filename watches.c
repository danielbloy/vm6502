#include "watches.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "fake6502.h"
#include "dissassembler.h"
#include "util.h"

typedef struct {
    watch_t watch;
    bool enabled;
} internalWatch_t;

// Watches need an address and length and type.
static internalWatch_t watches[MAX_WATCHES] = {0};
static size_t numWatches = 0;

// Simply returns the number of watches that have been added.
size_t countWatches() {
    return numWatches;
}

// Can add up to MAX_WATCHES watches.
size_t addWatch(watch_t watch) {
    if (numWatches >= MAX_WATCHES) {
        printf("ERROR: Cannot add watch as the maximum number of watches (%d) has been reached!\n", MAX_WATCHES);
        return 0;
    }

    for (size_t i = 0; i < MAX_WATCHES; ++i) {
        if (!watches[i].enabled) {
            watches[i].watch = watch;
            watches[i].enabled = true;
            ++numWatches;
            break;
        }
    }
    return numWatches;
}

// Add a watch in string form of <address><type><length>.
// If no length is specified then it defaults to 16 except for strings which is 64.
// Returns the number of watches added or zero on error.
size_t addWatchString(char const *str) {
    watch_t watch = {0, 0x10, hex};

    // Copy string as we will mutate.
    char buffer[64];
    strncpy(buffer, str, sizeof(buffer)/sizeof(char));
    buffer[sizeof(buffer)/sizeof(char) - 1] = 0;

    // We split each string into substrings.
    char *hyphen = strchr(buffer, '-');
    if (hyphen) *hyphen = 0; // Make it a terminal character.

    char *address = buffer;
    while(*address && isspace(*address)) ++address; // trim leading space

    if (isHexNumber(address)) {
        watch.address = (uint16_t) strtol(buffer, NULL, 16);
    } else {
        // TODO: Check for label
        printf("ERROR: The value '%s' specified for the address in the watch '%s' is not a valid hexadecimal number!\n", address, str);
        return 0;
    }

    // Now look for type (this should be safe as we have padding);
    char *type = hyphen + 1;
    if (hyphen && *type != 0) {
        while(*type && isspace(*type)) ++type; // trim leading space
        switch (*type) {
            case 'a':
            case 'A':
                watch.type = assembler;
                break;
            case 'c':
            case 'C':
                watch.type = ascii;
                break;
            case 'h':
            case 'H':
                watch.type = hex;
                break;
            case 's':
            case 'S':
                watch.type = string;
                watch.length = 0x40; // Default length for string.
                break;
            default:
                printf("ERROR: The value '%c' specified for the type in the watch '%s' is not one of a, A, c, C, h, H, s or S!\n", *type, str);
                return 0;
        }

        hyphen = strchr(type, '-');
        if (hyphen) *hyphen = 0; // Make it a terminal character.

        // Now do the length.
        char *length = hyphen + 1;
        if (hyphen && *length != 0) {
            if (!isHexNumber(length)) {
                printf("ERROR: The value '%s' specified for the length in the watch '%s' is not a valid hexadecimal number!\n", length, str);
                return 0;
            }
            watch.length = (uint16_t)strtol(length, NULL, 16);
        }
    }

    return addWatch(watch);
}

// Removes the watch numbered n.
size_t removeWatch(size_t n) {
    int num = 0;
    for(int w = 0; w < MAX_WATCHES; ++w) {
        if (!watches[w].enabled) continue;
        ++num;
        if (num == n) {
            watches[w].enabled = false;
            --numWatches;
            return numWatches;
        }
    }

    // If we got here we couldn't find the watch to remove.
    printf("ERROR: Cannot find watch number %zx!\n", n);
    return 0;
}

// Prints out the defined watches in 16 byte chunks.
void listWatches(FILE *fp, uint8_t *ram) {
    int num = 0;
    for(int w = 0; w < MAX_WATCHES; ++w) {
        if (!watches[w].enabled) continue;
        ++num;
        // Get the watch and loop over the memory addressed.
        watch_t *watch = &watches[w].watch;
        uint16_t address = watch->address;
        for (size_t i = 0; i < watch->length; ++i) {
            if (address >= MAX_ADDRESS) break;

            // We print out 16 bytes for hex and ascii, 64 for string and 1(ish) for assembler.
            const int rowLength = watch->type == hex || watch->type == ascii  ? 0x10 : watch->type == string ? 0x40 : 0x01;
            if (i % rowLength == 0) {
                // We need to put a new line on all but the first line.
                if (i) printf("\n");
                fprintf(fp, "WATCH %02x : $%04x: ", num, address);
            }
            if (watch->type == hex) fprintf(fp, "$%02x ", ram[address]);
            if (watch->type == ascii) fprintf(fp, "'%c' ", ram[address]);
            if (watch->type == string) {
                if (ram[address] == 0 || ram[address] == 10 || ram[address] == 13) break; // Null, CR or LF indicate end of string.
                fprintf(fp, "%c", ram[address]);
            }
            if (watch->type == assembler) {
                const diassembleResult_t dasm = dissassemble(&ram[address]); // This call could exceed the array bounds.
                fprintf(fp, "%s", dasm.assembler);
                // We may now need to advance the address and i;
                if (dasm.length > 1) {++address; ++i;}
                if (dasm.length > 2) {++address; ++i;}
            }
            ++address;
        }
        fprintf(fp, "\n");
    }
}
