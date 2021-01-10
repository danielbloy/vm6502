#pragma ide diagnostic ignored "hicpp-signed-bitwise"

#include <ctype.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "input.h"
#include "watches.h"
#include "util.h"
#include "breakpoints.h"

#define BUFFER_SIZE 1024

// Just reads in a binary file.
loadFile_t loadBinFile(FILE *file, uint8_t *ram, uint16_t loadAddress) {

    bool loadedAnything = false;
    loadFile_t result = {loadAddress, loadAddress, false};
    uint8_t buffer[BUFFER_SIZE];

    while (!feof(file)) {
        /* Read in 256 8-bit numbers into the buffer */
        size_t bytes = fread(buffer, sizeof(uint8_t), BUFFER_SIZE, file);
        if (bytes) loadedAnything = true;
        for (size_t i = 0; i < bytes; ++i, ++result.stopAddress) ram[result.stopAddress] = buffer[i];
    }

    if (loadedAnything) --result.stopAddress; // Remember to backpedal one byte but only if we loaded anything.
    return result;
}

void exitWithSyntaxError(size_t line, char const * const message, char const * const value) {
    printf("SYNTAX ERROR at line %zu: %s - '%s'\n", line, message, value);
    exit(1);
}

// Label definitions. Labels can only be used after being defined.
#define MAX_LABELS 1024
#define MAX_LABEL_LENGTH 64
typedef struct {
    char label[MAX_LABEL_LENGTH + 1];
    uint16_t address;
    uint8_t low;
    uint8_t high;
} Label_t;
static Label_t labels[MAX_LABELS] = {0};
static size_t labelCount = 0;


// Converts the hex specified string from the entry into a number and returns it.
uint16_t getHexNumberFromEntry(size_t line, char const *str) {
    char const *number = strchr(str, ':');
    ++number;
    if (isHexNumber(number)) return (uint16_t)strtol(number, NULL, 16);

    // Check if it is a label.
    if (labelCount) {
        // Advance pointer to strip leading space.
        while(isspace((unsigned char)*number)) ++number;
        // Count the number of characters, ignoring trailing space.
        size_t chars = 0;
        while(!isspace((unsigned char)number[chars])) ++chars;

        for (size_t i = 0; i < labelCount; ++i) {
            if (strncasecmp(number, labels[i].label, chars) == 0) {
                return labels[i].address;
            }
        }
    }

    exitWithSyntaxError(line, "The value is not a valid hexadecimal number!", number);
    return 0;
}

// If the input buffer contains a single byte label (prefixed with >, < or ^) then modify the buffer.
// The input buffer is expected to have all leading and trailing space removed.
bool singleByteLabel(char buffer[], uint16_t loadAddress) {
    if (!labelCount) return false;
    size_t length = strlen(buffer);
    if (length > MAX_LABEL_LENGTH) length = MAX_LABEL_LENGTH;
    if (length < 3) return false;
    if (buffer[0] != '>' && buffer[0] !='<' && buffer[0] !='^') return false;
    bool const relative = buffer[0] == '^';
    bool const higher = buffer[0] == '<';
    ++buffer;
    if (buffer[0] != '.') return false;

    for (size_t i = 0; i < labelCount; ++i) {
        if (strncasecmp(buffer, labels[i].label, length) == 0) {
            buffer--; // Make sure to overwrite the > or < character.
            if (relative) {
                int diff = labels[i].address - (loadAddress + 1);
                uint8_t reladdress = diff & 0x7F;
                if (diff < 0) reladdress |= 0x80;
                sprintf(buffer, "%02x", reladdress);
            } else {
                sprintf(buffer, "%02x", higher ? labels[i].high : labels[i].low);
            }
            return true;
        }
    }
    return false;
}

// If the input buffer contains a double byte label (name without prefix) then return a pointer to the label.
// The input buffer is expected to have all leading and trailing space removed.
Label_t * doubleByteLabel(char buffer[]) {
    if (!labelCount) return NULL;
    size_t length = strlen(buffer);
    if (length > MAX_LABEL_LENGTH) length = MAX_LABEL_LENGTH;
    if (length < 3) return NULL;
    if (buffer[0] != '.') return NULL;

    for (size_t i = 0; i < labelCount; ++i) {
        if (strncasecmp(buffer, labels[i].label, length) == 0) {
            return &labels[i];
        }
    }
    return NULL;
}

#define MAX_USER_SUBSTITUTIONS 1024
static struct {
    char match[MAX_LABEL_LENGTH + 1];
    char substitute[MAX_LABEL_LENGTH + 1];
} userSubstitutions[MAX_USER_SUBSTITUTIONS] = {0};
static size_t userSubstitutionsCount = 0;

// Special label for #define name value
void addSubstitution(size_t line, char *substitution) {
    if (userSubstitutionsCount == MAX_USER_SUBSTITUTIONS) exitWithSyntaxError(line, "Maximum number of user substitutions exceeded.", substitution);

    // Strip any leading space
    while(isspace(*substitution)) ++substitution;

    // Now extract the match part.
    char const *matchStart = substitution;
    while(!isspace(*substitution)) ++substitution;

    // Terminate the label and copy
    *substitution = 0;
    strncpy(userSubstitutions[userSubstitutionsCount].match, matchStart, MAX_LABEL_LENGTH);
    userSubstitutions[userSubstitutionsCount].match[MAX_LABEL_LENGTH] = 0;

    // Now copy the substitution; yes I know this is dangerous!
    ++substitution;
    // Strip any leading space
    while(isspace(*substitution)) ++substitution;
    char const *substitutionStart = substitution;
    while(!isspace(*substitution)) ++substitution;
    *substitution = 0;
    strncpy(userSubstitutions[userSubstitutionsCount].substitute, substitutionStart, MAX_LABEL_LENGTH);
    userSubstitutions[userSubstitutionsCount].substitute[MAX_LABEL_LENGTH] = 0;

    ++userSubstitutionsCount;
}

bool substitute(char buffer[]) {
    static struct {
        char const * const match;
        char const * const substitute;
    } substitutions[] = {
        // ASCII codes
        {"NULL", "00"},
        {"NUL", "00"}, // Null
        {"SOH", "01"}, // Start of heading
        {"STX", "02"}, // Start of text - not to be confused with STX instruction
        {"ETX", "03"}, // End of text
        {"EOT", "04"}, // End of transmit
        {"ENQ", "05"}, // Enquiry
        {"ACK", "06"}, // Acknowledge
        {"BEL", "07"}, // Bell
        {"BS", "08"},  // Backspace
        {"HT", "09"},  // Horizontal tab
        {"LF", "0A"},  // Line feed
        {"VT", "0B"},  // Vertical tab
        {"FF", "0C"},  // Form feed
        {"CR", "0D"},  // Carriage return
        {"SO", "1E"},  // Shift out
        {"SI", "0F"},  // shift in
        {"DLE", "10"}, // Data line escape
        {"DC1", "11"}, // Device control 1
        {"DC2", "12"}, // Device control 2
        {"DC3", "13"}, // Device control 3
        {"DC4", "14"}, // Device control 4
        {"NAK", "15"}, // Negative acknowledge
        {"SYN", "16"}, // Synchronous idle
        {"ETB", "17"}, // End of transmit block
        {"CAN", "18"}, // Cancel
        {"EM", "19"},  // End of medium
        {"SUB", "1A"}, // Substitute
        {"ESC", "1B"}, // Escape
        {"FS", "1C"},  // File separator
        {"GS", "1D"},  // Group separator
        {"RS", "1E"},  // Record separator
        {"US", "1F"},  // Unit separator

        // Operating system codes
        {"STDIN_BLOCK",   "FA"},
        {"STDIN_NOBLOCK", "FB"},
        {"STDOUT",        "FA"},
        {"STDERR",        "FB"},
        {"RANDOM",        "FC"},
        {"WRITE_Y",       "84"}, // STY zpg
        {"WRITE_A",       "85"}, // STA zpg
        {"WRITE_X",       "86"}, // STX zpg
        {"READ_Y",        "A4"}, // LDY zpg
        {"READ_A",        "A5"}, // LDA zpg
        {"READ_X",        "A6"}, // LDX zpg

        {"OS_CALL",          "FF"}, // Address
        {"OS_STDOUT",        "01"},
        {"OS_STDERR",        "02"},
        {"OS_PRINT_SCREEN",  "03"},
        {"OS_STR_TO_INT32",  "04"},
        {"OS_UINT8_TO_STR",  "05"},
        {"OS_INT8_TO_STR",   "06"},
        {"OS_UINT16_TO_STR", "07"},
        {"OS_INT16_TO_STR",  "08"},
        {"OS_UINT24_TO_STR", "09"},
        {"OS_INT24_TO_STR",  "0A"},
        {"OS_UINT32_TO_STR", "0B"},
        {"OS_INT32_TO_STR",  "0C"},
        {"OS_EXIT_SUCCESS",  "FE"},
        {"OS_EXIT_FAILURE",  "FF"},

        // Machine opcodes
        {"BRK",   "00"}, {"ORA_(z,X)", "01"},
        {"BPL",   "10"}, {"ORA_(z),Y", "11"},
        {"JSR_a", "20"}, {"AND_(z,X)", "21"},
        {"BML",   "30"}, {"AND_(z),Y", "31"},
        {"RTI",   "40"}, {"EOR_(z,X)", "41"},
        {"BVC",   "50"}, {"EOR_(z),Y", "51"},
        {"RTS",   "60"}, {"ADC_(z,X)", "61"},
        {"BVS",   "70"}, {"ADC_(z),Y", "71"},
                                         {"STA_(z,X)", "81"},
        {"BCC",   "90"}, {"STA_(z),Y", "91"},
        {"LDY_#", "A0"}, {"LDA_(z,X)", "A1"},
        {"BCS",   "B0"}, {"LDA_(z),Y", "B1"},
        {"CPY_#", "C0"}, {"CMP_(z,X)", "C1"},
        {"BNE",   "D0"}, {"CMP_(z),Y", "D1"},
        {"CPX_#", "E0"}, {"SBC_(z,X)", "E1"},
        {"BEQ",   "F0"}, {"SBC_(z),Y", "F1"},

        {"LDX_#", "A2"},

        {"BIT_z",   "24"},
        {"STY_z",   "84"},
        {"STY_z,X", "94"},
        {"LDY_z",   "A4"},
        {"LDY_z,X", "B4"},
        {"CPY_z",   "C4"},
        {"CPX_z",   "E4"},

        {"ORA_z",   "05"}, {"ASL_z",   "06"},
        {"ORA_z,X", "15"}, {"ASL_z,X", "16"},
        {"AND_z",   "25"}, {"ROL_z",   "26"},
        {"AND_z,X", "35"}, {"ROL_z,X", "36"},
        {"EOR_z",   "45"}, {"LSR_z",   "46"},
        {"EOR_z,X", "55"}, {"LSR_z,X", "56"},
        {"ADC_z",   "65"}, {"ROR_z",   "66"},
        {"ADC_z,X", "75"}, {"ROR_z,X", "76"},
        {"STA_z",   "85"}, {"STX_z",   "86"},
        {"STA_z,X", "95"}, {"STX_z,Y", "96"},
        {"LDA_z",   "A5"}, {"LDX_z",   "A6"},
        {"LDA_z,X", "B5"}, {"LDX_z,Y", "B6"},
        {"CMP_z",   "C5"}, {"DEC_z",   "C6"},
        {"CMP_z,X", "D5"}, {"DEC_z,X", "D6"},
        {"SBC_z",   "E5"}, {"INC_z",   "E6"},
        {"SBC_z,X", "F5"}, {"INC_z,X", "F6"},

        {"PHP", "08"}, {"ORA_#",   "09"},
        {"CLC", "18"}, {"ORA_a,Y", "19"},
        {"PLP", "28"}, {"AND_#",   "29"},
        {"SEC", "38"}, {"AND_a,Y", "39"},
        {"PHA", "48"}, {"EOR_#",   "49"},
        {"CLI", "58"}, {"EOR_a,Y", "59"},
        {"PLA", "68"}, {"ADC_#",   "69"},
        {"SEI", "78"}, {"ADC_a,Y", "79"},
        {"DEY", "88"},
        {"TYA", "98"}, {"STA_a,Y", "99"},
        {"TAY", "A8"}, {"LDA_#",   "A9"},
        {"CLV", "B8"}, {"LDA_a,Y", "B9"},
        {"INY", "C8"}, {"CMP_#",   "C9"},
        {"CLD", "D8"}, {"CMP_a,Y", "D9"},
        {"INX", "E8"}, {"SBC_#",   "E9"},
        {"SED", "F8"}, {"SBC_a,Y", "F9"},                              

        {"ASL", "0A"},                                     
        {"ROL", "2A"}, {"BIT_a",   "2C"},
        {"LSR", "4A"}, {"JMP_a",   "4C"},
        {"ROR", "6A"}, {"JMP_(a)", "6C"},
        {"TXA", "8A"}, {"STY_a",   "8C"},
        {"TXS", "9A"},
        {"TAX", "AA"}, {"LDY_a",   "AC"},
        {"TSX", "BA"}, {"LDY_a,X", "BC"},
        {"DEX", "CA"}, {"CPY_a",   "CC"},
        {"NOP", "EA"}, {"CPX_a",   "EC"},

        {"ORA_a",   "0D"}, {"ASL_a",   "0E"},
        {"ORA_a,X", "1D"}, {"ASL_a,X", "1E"},
        {"AND_a",   "2D"}, {"ROL_a",   "2E"},
        {"AND_a,X", "3D"}, {"ROL_a,X", "3E"},
        {"EOR_a",   "4D"}, {"LSR_a",   "4E"},
        {"EOR_a,X", "5D"}, {"LSR_a,X", "5E"},
        {"ADC_a",   "6D"}, {"ROR_a",   "6E"},
        {"ADC_a,X", "7D"}, {"ROR_a,X", "7E"},
        {"STA_a",   "8D"}, {"STX_a",   "8E"},
        {"STA_a,X", "9D"},
        {"LDA_a",   "AD"}, {"LDX_a",   "AE"},
        {"LDA_a,X", "BD"}, {"LDX_a,y", "BE"},
        {"CMP_a",   "CD"}, {"DEC_a",   "CE"},
        {"CMP_a,X", "DD"}, {"DEC_a,X", "DE"},
        {"SBC_a",   "ED"}, {"INC_a",   "EE"},
        {"SBC_a,X", "FD"}, {"INC_a,X", "FE"},
    };

    // Try built in substitutions first
    for (size_t i = 0, count = (sizeof substitutions / sizeof substitutions[0]); i < count; ++i) {
        if (strcasecmp(buffer, substitutions[i].match) == 0) {
            if (verbose) printf("INPUT .. : Substituting '%s' for '%s'.\n", buffer, substitutions[i].substitute);
            strcpy(buffer, substitutions[i].substitute);
            return true;
        }
    }

    // Now try user substitutions
    for (size_t i = 0; i < userSubstitutionsCount; ++i) {
        if (strcasecmp(buffer, userSubstitutions[i].match) == 0) {
            if (verbose) printf("INPUT .. : Substituting '%s' for user value '%s'.\n", buffer, userSubstitutions[i].substitute);
            strcpy(buffer, userSubstitutions[i].substitute);
            return true;
        }
    }

    return false;
}

// Reads a line at a time.
loadFile_t loadHexFile(FILE *file, uint8_t *ram, uint16_t loadAddress) {

    bool loadedAnything = false;
    loadFile_t result = {loadAddress, loadAddress, false}; // Default start and stop to the load address.
    char buffer[BUFFER_SIZE];

    bool startSpecified = false;
    bool stopSpecified = false;

    size_t line = 0;
    while (!feof(file)) {
        if (!fgets(buffer, BUFFER_SIZE, file)) break;
        char *str = buffer;
        ++line;

        // Kill any CR or LF characters
        {
            char *lf = strchr(buffer, '\n');
            if (lf) *lf = 0;
            char *cr = strchr(buffer, '\r');
            if (cr) *cr = 0;
        }

        // Strip all leading white space.
        while(isspace((unsigned char)*str)) str++;
        if (*str == 0) continue; // All spaces so ignore it?
        if (*str == ';') continue; // If it starts with a comment then ignore it.

        // We have to do strings first as we don't want to strip out the ';' character.
        // NOTE: We do NOT use strings to set the start address as it makes no sense.
        if (strncasecmp("STR:", str, 4) == 0 || strncasecmp("STRING:", str, 7) == 0) {
            str += strncasecmp("STRING:", str, 7) ? 4 : 7; // Advance pointer so we can ignore it.
            while(strlen(str)) {
                ram[loadAddress] = *str;
                ++str;
                ++loadAddress;
            }
            // Add the null terminator
            ram[loadAddress] = 0;
            ++loadAddress;
            continue;
        }

        // Check for ; and kill the string at that point to strip out comments.
        // We do have to check for the special case of a quoted semi-colon ';' though.
        {
            char *comment = str;
            do {
                comment = strchr(comment, ';');
                if (comment) {
                    // This should be safe as we've already eliminated lines starting with a ; character
                    // and the string will be at least null terminated.
                    char *before = comment - 1;
                    char *after = comment + 1;
                    if (*before == '\'' && *after == '\'') {
                        // If this is a genuine quoted character then we just have to advance to check for a comment.
                        comment = after + 1;
                    } else {
                        *comment = 0;
                        comment = NULL;
                    }
                }
            } while (comment);
        }

        // Check for label definitions.
        if (str[0] == '.') {
            if (labelCount == MAX_LABELS) exitWithSyntaxError(line, "Maximum number of labels exceeded.", str);
            Label_t *label = &labels[labelCount];

            // Count the number of characters, ignoring trailing space.
            size_t chars = 0;
            while(!isspace((unsigned char)str[chars])) ++chars;

            strncpy(label->label, str, MAX_LABEL_LENGTH);
            label->label[chars] = 0; // Remove any trailing spaces.
            label->label[MAX_LABEL_LENGTH] = 0; // Safety terminator.
            label->address = loadAddress;
            label->low  = loadAddress & 0xFF;
            label->high = (loadAddress & 0xFF00) >> 8;
            ++labelCount;
            continue;
        }

        // Check for switches.
        if (strncasecmp("#INFO",         str, 5)  == 0) { info = true; continue; }
        if (strncasecmp("#VERBOSE",      str, 8)  == 0) { info = true; verbose = true; continue; }
        if (strncasecmp("#NOFINALSTATE", str, 13) == 0) { finalState = false; continue; }
        if (strncasecmp("#DUMPCPU",      str, 8 ) == 0) { dumpCpu = true; continue; }
        if (strncasecmp("#DUMPSCREEN",   str, 11) == 0) { dumpScreen = true; continue; }
        if (strncasecmp("#DUMPWATCHES",  str, 12) == 0) { dumpWatches = true; continue; }
        if (strncasecmp("#NOAUTOSTOP",   str, 11) == 0) { result.disableAutoStop = true; continue; }
        if (strncasecmp("#SCREENMODE1",  str, 12) == 0) { screenMode = 1; continue; }
        if (strncasecmp("#DEFINE",       str, 7)  == 0) { addSubstitution(line, &str[7]); continue; }

        // Check for START:, STOP: and BREAK: as these might be followed by a single number (and possibly a comment).
        // If no number, then their value is the current loadAddress.
        if (strncasecmp("START:", str, 6) == 0) {
            result.startAddress = getHexNumberFromEntry(line, str);
            if (isEmpty(&str[6])) result.startAddress = loadAddress; // If nothing specified then use load Address
            startSpecified = true;
            continue;
        }
        if (strncasecmp("STOP:", str, 5) == 0) {
            result.stopAddress = getHexNumberFromEntry(line, str);
            if (isEmpty(&str[5])) result.stopAddress = loadAddress; // If nothing specified then use load Address
            stopSpecified = true;
            continue;
        }
        if (strncasecmp("BREAK:", str, 6) == 0) {
            if (isEmpty(&str[6])) {
                addBreakpoint(loadAddress); // If nothing specified then use load Address
            } else {
                addBreakpoint(getHexNumberFromEntry(line, str));
            }
            continue;
        }
        // Check for ORG: this must be followed by a single number (and possibly a comment).
        if (strncasecmp("ORG:", str, 4) == 0) {
            loadAddress = getHexNumberFromEntry(line, str);
            if (!startSpecified) { // The first time we specify this we default it as our start address.
                result.startAddress = loadAddress;
                startSpecified = true;
            }
            continue;
        }

        // Support specifying of watches.
        if (strncasecmp("WATCH:", str, 6) == 0) {
            if (!addWatchString(&str[6])) {
                exitWithSyntaxError(line, "Invalid watch definition", str);
            }
            continue;
        }

        // Check for HEX: And advance pointer as we can ignore it.
        if (strncasecmp("HEX:", str, 4) == 0) str += 4;
        // Check for <number>: as that indicates an ORG followed by data. We split the string into two.
        // We do however need to check for the special case of a quoted colon ':'.
        {
            char *colon = strchr(str, ':');
            if (colon && colon != str && *(colon-1) == '\'' && *(colon+1) == '\'') {
                colon = NULL;
            }

            if (colon) {
                *colon = 0; // Split the string into two.
                // Validate only hex numbers:
                if (!isHexNumber(str)) {
                    exitWithSyntaxError(line, "Unexpected string before colon", str);
                }
                loadAddress = (uint16_t)strtol(str, NULL, 16);
                str = colon + 1; // Advance the string to after the new null character.
                if (!startSpecified) { // The first time we specify this we default it as our start address.
                    result.startAddress = loadAddress;
                    startSpecified = true;
                }
            }
        }

        // Everything left should be a string of numbers, characters (in single quotes) or labels separated by whitespace.
        // We strip and process each in turn.
        while(isspace((unsigned char)*str)) ++str;
        while(strlen(str)) {
            if (!startSpecified) { // The first time we load, we implicitly set it as our start address.
                result.startAddress = loadAddress;
                startSpecified = true;
            }

            // We make a copy to validate as we want to terminate after the hex number as it
            // may be followed by another hex number.
            char number[64];
            strncpy(number, str, 64);
            number[63] = 0;
            { // Now terminate after the hex characters
                char *numend = number;
                while (*numend && !isspace((unsigned char)*numend)) ++numend;
                while (*str    && !isspace((unsigned char)*str))    ++str;
                // We have to treat the ' ' triplet as special here.
                if (number[0] == '\'' && strlen(number) >2 && number[1] == ' ' && number[2] == '\'') {
                    numend += 2;
                    str += 2; // We need to do this here.
                }
                *numend = 0;
            }

            // If it is a quoted character then load the value directly.
            if (strlen(number) == 3 && number[0] == '\'' && number[2] == '\'') {
                ram[loadAddress] = number[1];
            } else {
                // If it is a number then we use it directly, otherwise we look up to see if it is
                // a defined name for substitution.
                if (substitute(number) || isHexNumber(number) || singleByteLabel(number, loadAddress)) { // If this returns true then it modifies the number buffer.
                    uint8_t data = (uint8_t)strtol(number, NULL, 16);
                    ram[loadAddress] = data;
                } else {
                    Label_t * label = doubleByteLabel(number);
                    if (!label) {
                        exitWithSyntaxError(line, "Not a valid hexadecimal number", number);
                    }
                    ram[loadAddress] = label->low;
                    ++loadAddress;
                    ram[loadAddress] = label->high;
                }
            }
            loadedAnything = true;
            ++loadAddress;
            while(isspace((unsigned char)*str)) ++str; // Strip whitespace before character.
        }
    }

    if (loadedAnything) --loadAddress; // Remember to backpedal one byte but only if we loaded anything.
    if (!stopSpecified) result.stopAddress = loadAddress;
    return result;
}
