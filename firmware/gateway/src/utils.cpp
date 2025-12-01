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

uint8_t calculate_checksum(const uint8_t* data, size_t length) {
    uint8_t checksum = 0;
    for (size_t i = 0; i < length - 1; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

bool verify_checksum(const uint8_t* data, size_t length) {
    uint8_t calculated = calculate_checksum(data, length);
    return calculated == data[length - 1];
}

int16_t encode_temperature(float temp) {
    return static_cast<int16_t>(temp * 100.0f);
}

float decode_temperature(int16_t encoded) {
    return encoded / 100.0f;
}

uint16_t encode_humidity(float humidity) {
    return static_cast<uint16_t>(humidity * 100.0f);
}

float decode_humidity(uint16_t encoded) {
    return encoded / 100.0f;
}
