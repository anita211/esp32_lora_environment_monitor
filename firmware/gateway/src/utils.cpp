#include "utils.h"
#include "constants.h"
#include <stdarg.h>
#include <stdio.h>

#ifdef DEBUG
void print_log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}
#else
void print_log(const char* format, ...) {}
#endif

