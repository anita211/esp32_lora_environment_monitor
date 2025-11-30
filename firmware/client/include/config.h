/**
 * @file config.h
 * @brief Configuration settings for ESP32 LoRa Environment Monitor Client
 * 
 * This file contains all configurable parameters for the sensor node.
 * Adjust these values according to your hardware setup and requirements.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

/* ============================================================================
 * NODE IDENTIFICATION
 * ============================================================================ */

#define NODE_ID                 1           // Unique ID for this sensor node (1-255)

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
 * LORA RADIO - RF PARAMETERS
 * ============================================================================ */

#define LORA_FREQUENCY_MHZ      915.0       // RF frequency: 915 (Americas), 868 (EU), 433 (Asia)
#define LORA_BANDWIDTH_KHZ      125.0       // Bandwidth: 125, 250, or 500 kHz
#define LORA_SPREADING_FACTOR   9           // Spreading factor: 7-12 (higher = longer range, slower)
#define LORA_CODING_RATE        7           // Coding rate: 5-8 (higher = more error correction)
#define LORA_SYNC_WORD          0x12        // Network sync word (must match gateway)
#define LORA_TX_POWER_DBM       20          // Transmit power in dBm (2-20)
#define LORA_PREAMBLE_LENGTH    8           // Preamble symbols

/* ============================================================================
 * TRANSMISSION SETTINGS
 * ============================================================================ */

#define TX_INTERVAL_MS          30000       // Time between transmissions (milliseconds)
#define TX_MAX_RETRIES          3           // Max retry attempts on TX failure

// Adaptive transmission (skip TX if values haven't changed significantly)
#define ADAPTIVE_TX_ENABLED     false       // Enable/disable adaptive transmission
#define HUMIDITY_CHANGE_THRESHOLD   2.0     // Min humidity change (%) to trigger TX
#define DISTANCE_CHANGE_THRESHOLD   10.0    // Min distance change (cm) to trigger TX

/* ============================================================================
 * POWER MANAGEMENT
 * ============================================================================ */

#define DEEP_SLEEP_ENABLED      true        // Use deep sleep between transmissions
#define DEEP_SLEEP_TIME_US      (TX_INTERVAL_MS * 1000ULL)  // Sleep duration in microseconds

/* ============================================================================
 * SENSORS - GENERAL
 * ============================================================================ */

#define REAL_SENSORS_ENABLED    false       // true = real hardware, false = simulation

/* ============================================================================
 * SENSOR - HC-SR04 ULTRASONIC (Distance/Presence)
 * ============================================================================ */

#define PIN_ULTRASONIC_TRIG     1           // Trigger pin (GPIO1 / D0)
#define PIN_ULTRASONIC_ECHO     2           // Echo pin (GPIO2 / D1) - use voltage divider!
#define ULTRASONIC_TIMEOUT_US   30000       // Measurement timeout (microseconds)
#define PRESENCE_DISTANCE_CM    100         // Distance threshold for presence detection

/* ============================================================================
 * SENSOR - MH-RD SOIL MOISTURE
 * ============================================================================ */

#define PIN_SOIL_MOISTURE       3           // Analog input pin (GPIO3 / A2)
#define SOIL_MOISTURE_SAMPLES   10          // Number of readings to average
#define SOIL_ADC_DRY            4095        // ADC value when sensor is dry (calibrate!)
#define SOIL_ADC_WET            1500        // ADC value when sensor is wet (calibrate!)

/* ============================================================================
 * SENSOR - BH1750 LUMINOSITY (I2C)
 * ============================================================================ */

#define PIN_I2C_SDA             4           // I2C Data pin (GPIO4 / D3)
#define PIN_I2C_SCL             5           // I2C Clock pin (GPIO5 / D4)
#define BH1750_I2C_ADDRESS      0x23        // BH1750 default I2C address (ADDR pin low)
// Alternative address: 0x5C when ADDR pin is high

/* ============================================================================
 * SIMULATION MODE - Default Values
 * When REAL_SENSORS_ENABLED = false, these values are used as base + variation
 * ============================================================================ */

#define SIM_HUMIDITY_BASE       55.0        // Base humidity value (%)
#define SIM_HUMIDITY_VARIATION  35.0        // +/- variation range
#define SIM_DISTANCE_BASE       150.0       // Base distance value (cm)
#define SIM_DISTANCE_VARIATION  120.0       // +/- variation range
#define SIM_TEMPERATURE_BASE    25.0        // Base temperature value (°C)
#define SIM_TEMPERATURE_VARIATION 10.0      // +/- variation range
#define SIM_LUMINOSITY_BASE     500.0       // Base luminosity value (lux)
#define SIM_LUMINOSITY_VARIATION 400.0      // +/- variation range

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

#define CLIENT_ID               NODE_ID
#define LORA_MOSI               PIN_LORA_MOSI
#define LORA_MISO               PIN_LORA_MISO
#define LORA_SCK                PIN_LORA_SCK
#define LORA_NSS                PIN_LORA_NSS
#define LORA_RST                PIN_LORA_RST
#define LORA_DIO1               PIN_LORA_DIO1
#define LORA_BUSY               PIN_LORA_BUSY
#define LORA_FREQUENCY          LORA_FREQUENCY_MHZ
#define LORA_BANDWIDTH          LORA_BANDWIDTH_KHZ
#define LORA_SPREADING          LORA_SPREADING_FACTOR
//#define LORA_CODING_RATE        LORA_CODING_RATE
#define LORA_TX_POWER           LORA_TX_POWER_DBM
#define LORA_PREAMBLE           LORA_PREAMBLE_LENGTH
#define MAX_TX_RETRIES          TX_MAX_RETRIES
#define ENABLE_ADAPTIVE_TX      ADAPTIVE_TX_ENABLED
#define HUMID_THRESHOLD         HUMIDITY_CHANGE_THRESHOLD
#define DISTANCE_THRESHOLD      DISTANCE_CHANGE_THRESHOLD
#define ENABLE_DEEP_SLEEP       DEEP_SLEEP_ENABLED
#define SLEEP_TIME_US           DEEP_SLEEP_TIME_US
#define USE_REAL_SENSORS        REAL_SENSORS_ENABLED
#define ULTRASONIC_TRIG_PIN     PIN_ULTRASONIC_TRIG
#define ULTRASONIC_ECHO_PIN     PIN_ULTRASONIC_ECHO
#define PRESENCE_THRESHOLD_CM   PRESENCE_DISTANCE_CM
#define MOISTURE_SENSOR_PIN     PIN_SOIL_MOISTURE
#define MOISTURE_SAMPLES        SOIL_MOISTURE_SAMPLES
#define MOISTURE_DRY_VALUE      SOIL_ADC_DRY
#define MOISTURE_WET_VALUE      SOIL_ADC_WET
#define HUMID_BASE              SIM_HUMIDITY_BASE
#define HUMID_VARIATION         SIM_HUMIDITY_VARIATION
#define DISTANCE_BASE           SIM_DISTANCE_BASE
#define DISTANCE_VARIATION      SIM_DISTANCE_VARIATION
#define TEMP_BASE               SIM_TEMPERATURE_BASE
#define TEMP_VARIATION          SIM_TEMPERATURE_VARIATION
#define LUX_BASE                SIM_LUMINOSITY_BASE
#define LUX_VARIATION           SIM_LUMINOSITY_VARIATION
#define DEBUG_MODE              DEBUG_ENABLED
#define SERIAL_BAUD             SERIAL_BAUD_RATE
#define DEBUG_PRINT(x)          LOG(x)
#define DEBUG_PRINTLN(x)        LOG_LN(x)
#define DEBUG_PRINTF(...)       LOG_F(__VA_ARGS__)

#endif // CONFIG_H
