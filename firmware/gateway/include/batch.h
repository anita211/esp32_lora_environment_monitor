#ifndef BATCH_H
#define BATCH_H

#include <Arduino.h>
#include "constants.h"

#ifdef BATCH_ON
extern String batch_buffer[BATCH_SIZE];
extern uint8_t batch_count;
extern uint32_t batch_start_time;

void add_to_batch(const String& json);
void flush_batch();
#endif

#endif // BATCH_H
