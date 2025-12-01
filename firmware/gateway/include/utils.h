#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

void print_log(const char* format, ...);

uint8_t calculate_checksum(const uint8_t *data, size_t length);
bool verify_checksum(const uint8_t *data, size_t length);

int16_t encode_temperature(float temp);
float decode_temperature(int16_t encoded);

uint16_t encode_humidity(float humidity);
float decode_humidity(uint16_t encoded);

#endif
