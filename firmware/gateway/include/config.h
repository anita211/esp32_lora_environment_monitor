/**
 * @file config.h
 * @brief Configuration settings for ESP32 LoRa Environment Monitor Gateway
 * 
 * This file contains all configurable parameters for the gateway node.
 * The gateway receives data from sensor nodes via LoRa and forwards to server.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ============================================================================
 * GATEWAY IDENTIFICATION
 * ============================================================================ */

#define GATEWAY_ID              1           // Unique ID for this gateway (1-255)

/* ============================================================================
 * LORA RADIO - SPI PIN MAPPING (XIAO ESP32-S3)
 * ============================================================================ */

#define PIN_LORA_MOSI           9           // SPI Master Out Slave In
#define PIN_LORA_MISO           8           // SPI Master In Slave Out
#define PIN_LORA_SCK            7           // SPI Clock
#define PIN_LORA_NSS            41          // SPI Chip Select (CS)
#define PIN_LORA_RST            42          // LoRa module reset
#define PIN_LORA_DIO1           39          // LoRa interrupt (IRQ)
#define PIN_LORA_BUSY           40          // LoRa busy signal

/* ============================================================================
 * LORA RADIO - RF PARAMETERS (must match client nodes!)
 * ============================================================================ */

#define LORA_FREQUENCY_MHZ      915.0       // RF frequency: 915 (Americas), 868 (EU), 433 (Asia)
#define LORA_BANDWIDTH_KHZ      125.0       // Bandwidth: 125, 250, or 500 kHz
#define LORA_SPREADING_FACTOR   9           // Spreading factor: 7-12
#define LORA_CODING_RATE        7           // Coding rate: 5-8
#define LORA_SYNC_WORD          0x12        // Network sync word (must match clients)
#define LORA_PREAMBLE_LENGTH    8           // Preamble symbols

/* ============================================================================
 * WIFI CONFIGURATION
 * ============================================================================ */

#define WIFI_ENABLED            true        // Enable WiFi connectivity
#define WIFI_SSID               "YOUR_WIFI_SSID"
#define WIFI_PASSWORD           "YOUR_WIFI_PASSWORD"
#define WIFI_TIMEOUT_MS         10000       // Connection timeout (milliseconds)

/* ============================================================================
 * SERVER CONFIGURATION
 * ============================================================================ */

#define HTTP_ENABLED            true        // Enable HTTP forwarding to server
#define SERIAL_OUTPUT_ENABLED   true        // Enable serial output for debugging

#define SERVER_HOST             "192.168.1.100"  // Server IP address
#define SERVER_PORT             8080             // Server port
#define SERVER_ENDPOINT_DATA    "/api/sensor-data"
#define SERVER_ENDPOINT_STATS   "/api/gateway-stats"

/* ============================================================================
 * BATCH MODE - Group multiple messages before sending
 * ============================================================================ */

#define BATCH_MODE_ENABLED      true        // Enable batch transmission
#define BATCH_SIZE              5           // Max messages per batch
#define BATCH_TIMEOUT_MS        30000       // Send batch after this timeout

/* ============================================================================
 * STATISTICS & MONITORING
 * ============================================================================ */

#define STATS_INTERVAL_MS       60000       // Statistics report interval (60s)
#define MAX_PACKET_SIZE         256         // Maximum LoRa packet size

/* ============================================================================
 * TEST MODE - Simulate sensor data without real hardware
 * ============================================================================ */

#define TEST_MODE_ENABLED       false       // Enable test packet generation
#define TEST_INTERVAL_MS        15000       // Test packet interval

/* ============================================================================
 * DEBUG & SERIAL OUTPUT
 * ============================================================================ */

#define DEBUG_ENABLED           true        // Enable serial debug output
#define SERIAL_BAUD_RATE        115200      // Serial port baud rate

// Debug macros - compile out when disabled
#if DEBUG_ENABLED
    #define LOG(x)              Serial.print(x)
    #define LOG_LN(x)           Serial.println(x)
    #define LOG_F(...)          Serial.printf(__VA_ARGS__)
#else
    #define LOG(x)
    #define LOG_LN(x)
    #define LOG_F(...)
#endif

/* ============================================================================
 * BACKWARD COMPATIBILITY ALIASES
 * These maintain compatibility with existing code
 * ============================================================================ */

// LoRa pins
#define LORA_MOSI               PIN_LORA_MOSI
#define LORA_MISO               PIN_LORA_MISO
#define LORA_SCK                PIN_LORA_SCK
#define LORA_NSS                PIN_LORA_NSS
#define LORA_RST                PIN_LORA_RST
#define LORA_DIO1               PIN_LORA_DIO1
#define LORA_BUSY               PIN_LORA_BUSY

// LoRa parameters
#define LORA_FREQUENCY          LORA_FREQUENCY_MHZ
#define LORA_BANDWIDTH          LORA_BANDWIDTH_KHZ
#define LORA_SPREADING          LORA_SPREADING_FACTOR
#define LORA_PREAMBLE           LORA_PREAMBLE_LENGTH

// Feature flags
#define ENABLE_WIFI             WIFI_ENABLED
#define USE_HTTP                HTTP_ENABLED
#define USE_SERIAL              SERIAL_OUTPUT_ENABLED
#define ENABLE_BATCH_MODE       BATCH_MODE_ENABLED
#define TEST_MODE               TEST_MODE_ENABLED

// Debug macros
#define DEBUG_MODE              DEBUG_ENABLED
#define SERIAL_BAUD             SERIAL_BAUD_RATE
#define DEBUG_PRINT(x)          LOG(x)
#define DEBUG_PRINTLN(x)        LOG_LN(x)
#define DEBUG_PRINTF(...)       LOG_F(__VA_ARGS__)

#endif // CONFIG_H
