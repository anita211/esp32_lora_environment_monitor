#ifndef PROCESSING_H
#define PROCESSING_H

#include <Arduino.h>
#include "message_struct.h"

void process_rx_lora_message(
  uint8_t* data,
  size_t length,
  float rssi,
  float snr
);
void handle_sensor_data(SensorDataMessage* msg, float rssi, float snr);
uint32_t get_duplicate_count();

#endif // PROCESSING_H
