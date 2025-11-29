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
#define LORA_PREAMBLE 8

#define ENABLE_WIFI true
#define WIFI_SSID "MORAES_MATOS_2.4G"
#define WIFI_PASSWORD "Abzx@02468"
#define WIFI_TIMEOUT_MS 10000 

#define USE_HTTP true
#define USE_SERIAL true

#define SERVER_HOST "192.168.1.11"
#define SERVER_PORT 8080
#define SERVER_ENDPOINT "/api/sensor-data"

#define SERIAL_BAUD 115200
#define GATEWAY_ID 1
#define MAX_PACKET_SIZE 256
#define STATS_INTERVAL_MS 60000
#define DEBUG_MODE true
#define TEST_MODE true
#define TEST_INTERVAL_MS 15000

#define ENABLE_BATCH_MODE true
#define BATCH_SIZE 5
#define BATCH_TIMEOUT_MS 30000

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
