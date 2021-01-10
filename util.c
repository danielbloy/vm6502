#include <ctype.h>
#include "util.h"

// Checks that the string only contains characters specified by the valid func. Trims
// leading and trailing space.
bool isNumber(char const * str, int (*valid)(int)) {
    bool result = true;

    while(*str && isspace(*str)) ++str; // trim leading space

    while(*str) {
        if (isspace(*str)) break; // Exit if we find any trailing space

        if (!valid(*str)) {
            result = false;
            break;
        }
        ++str;
    }

    while(*str && isspace(*str)) ++str; // trim trailing space

    // If we find another character then fail.
    if (*str) result = false;

    return result;
}

// Returns whether the string pointed to contains only valid hexadecimal characters.
bool isHexNumber(char const * str) {
    return isNumber(str, isxdigit);
}

// Returns whether the string pointed to contains only valid hexadecimal characters.
bool isDecimalNumber(char const * str) {
    return isNumber(str, isdigit);
}

// Returns whether the string is just space or not.
bool isEmpty(char const *str) {
    while(*str) {
        if (!isspace(*str)) return false;
        ++str;
    }
    return true;
}