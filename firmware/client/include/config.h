#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

#define LORA_MOSI 9
#define LORA_MISO 8
#define LORA_SCK 7
#define LORA_NSS 41
#define LORA_RST 42
#define LORA_DIO1 39
#define LORA_BUSY 40

#define LORA_FREQUENCY 915.0
#define LORA_BANDWIDTH 125.0
#define LORA_SPREADING 9
#define LORA_CODING_RATE 7
#define LORA_SYNC_WORD 0x12
#define LORA_TX_POWER 20
#define LORA_PREAMBLE 8

#define CLIENT_ID 1
#define TX_INTERVAL_MS 30000
#define ENABLE_DEEP_SLEEP true
#define SLEEP_TIME_US (TX_INTERVAL_MS * 1000)

#define USE_REAL_SENSORS false
#define ULTRASONIC_TRIG_PIN 1
#define ULTRASONIC_ECHO_PIN 2
#define ULTRASONIC_TIMEOUT_US 30000
#define PRESENCE_THRESHOLD_CM 100

#define MOISTURE_SENSOR_PIN 3
#define MOISTURE_SAMPLES 10
#define MOISTURE_DRY_VALUE 4095
#define MOISTURE_WET_VALUE 1500

#define HUMID_BASE 55.0
#define HUMID_VARIATION 35.0
#define DISTANCE_BASE 150.0
#define DISTANCE_VARIATION 120.0

#define ENABLE_ADAPTIVE_TX false
#define HUMID_THRESHOLD 2.0
#define DISTANCE_THRESHOLD 10.0

#define MAX_TX_RETRIES 3

#define DEBUG_MODE true
#define SERIAL_BAUD 115200

#if DEBUG_MODE
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(...)
#endif

#endif
