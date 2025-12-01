#ifndef PROCESSING_H
#define PROCESSING_H

#include <Arduino.h>
#include "protocol.h"

void process_rx_lora_message(
  uint8_t* data,
  size_t length,
  float rssi,
  float snr
);
static String build_sensor_json(SensorDataMessage* msg, float rssi, float snr);
void handle_sensor_data(SensorDataMessage* msg, float rssi, float snr);

#endif // PROCESSING_H
