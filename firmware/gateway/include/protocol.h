/**
 * @file protocol.h
 * @brief Binary protocol definitions for LoRa communication
 * 
 * Defines message structures and utility functions for communication
 * between sensor nodes (clients) and the gateway.
 * 
 * All structs are packed to ensure consistent byte layout across platforms.
 * This file should be identical on both client and gateway.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

/* ============================================================================
 * MESSAGE TYPE IDENTIFIERS
 * ============================================================================ */

typedef enum {
    MSG_TYPE_SENSOR_DATA    = 0x01,     // Regular sensor data transmission
    MSG_TYPE_HEARTBEAT      = 0x02,     // Keep-alive / status message
    MSG_TYPE_ALERT          = 0x03,     // Alert/alarm notification
    MSG_TYPE_ACK            = 0xAA,     // Acknowledgment from gateway
} MessageType;

/* ============================================================================
 * NODE STATUS FLAGS (can be combined with bitwise OR)
 * ============================================================================ */

typedef enum {
    STATUS_OK               = 0x00,     // All systems normal
    STATUS_LOW_BATTERY      = 0x01,     // Battery below threshold
    STATUS_SENSOR_ERROR     = 0x02,     // Sensor read failure
    STATUS_LORA_ERROR       = 0x04,     // LoRa communication issue
} NodeStatus;

/* ============================================================================
 * ALERT CODES
 * ============================================================================ */

typedef enum {
    // Temperature alerts (0x1x)
    ALERT_TEMPERATURE_HIGH  = 0x10,
    ALERT_TEMPERATURE_LOW   = 0x11,
    
    // Humidity alerts (0x2x)
    ALERT_HUMIDITY_HIGH     = 0x20,
    ALERT_HUMIDITY_LOW      = 0x21,
    
    // Distance/presence alerts (0x3x)
    ALERT_DISTANCE_LOW      = 0x30,     // Object detected nearby
} AlertCode;

/* ============================================================================
 * MESSAGE STRUCTURES
 * All structures are packed for consistent binary representation
 * ============================================================================ */

/**
 * @brief Sensor data message (16 bytes)
 * 
 * Sent periodically with current sensor readings.
 * Temperature is encoded as value * 100 (e.g., 25.5°C = 2550)
 * Humidity is encoded as value * 100 (e.g., 65.5% = 6550)
 * Luminosity is in lux (0-65535)
 */
struct __attribute__((packed)) SensorDataMessage {
    uint8_t     msg_type;       // MSG_TYPE_SENSOR_DATA (0x01)
    uint8_t     client_id;      // Node identifier (1-255)
    uint32_t    timestamp;      // Milliseconds since boot
    int16_t     temperature;    // Temperature * 100 (°C) - from soil moisture sensor
    uint16_t    humidity;       // Humidity * 100 (%) - soil moisture
    uint16_t    distance_cm;    // Distance in centimeters
    uint8_t     battery;        // Battery level (0-100%)
    uint16_t    luminosity_lux; // Luminosity in lux (BH1750 sensor)
    uint8_t     reserved;       // Reserved for future use
    uint8_t     checksum;       // XOR checksum of all preceding bytes
};

/**
 * @brief Heartbeat message (8 bytes)
 * 
 * Lightweight keep-alive message to indicate node is active.
 */
struct __attribute__((packed)) HeartbeatMessage {
    uint8_t     msg_type;       // MSG_TYPE_HEARTBEAT (0x02)
    uint8_t     client_id;      // Node identifier
    uint32_t    timestamp;      // Milliseconds since boot
    uint8_t     status;         // NodeStatus flags
    uint8_t     checksum;       // XOR checksum
};

/**
 * @brief Alert message (11 bytes)
 * 
 * Sent when a sensor reading exceeds defined thresholds.
 */
struct __attribute__((packed)) AlertMessage {
    uint8_t     msg_type;       // MSG_TYPE_ALERT (0x03)
    uint8_t     client_id;      // Node identifier
    uint32_t    timestamp;      // Milliseconds since boot
    uint8_t     alert_code;     // AlertCode value
    int16_t     alert_value;    // The value that triggered the alert
    uint8_t     severity;       // Alert severity (1=low, 2=medium, 3=high)
    uint8_t     reserved;       // Reserved for future use
    uint8_t     checksum;       // XOR checksum
};

/* ============================================================================
 * BACKWARD COMPATIBILITY ALIASES
 * ============================================================================ */

#define ALERT_TEMP_HIGH         ALERT_TEMPERATURE_HIGH
#define ALERT_TEMP_LOW          ALERT_TEMPERATURE_LOW

/* ============================================================================
 * CHECKSUM UTILITIES
 * ============================================================================ */

/**
 * @brief Calculate XOR checksum for a message
 * @param data Pointer to message data
 * @param length Total message length (checksum byte will be excluded)
 * @return Calculated checksum value
 */
inline uint8_t calculate_checksum(const uint8_t* data, size_t length)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < length - 1; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * @brief Verify checksum of a received message
 * @param data Pointer to message data
 * @param length Total message length (including checksum byte)
 * @return true if checksum is valid, false otherwise
 */
inline bool verify_checksum(const uint8_t* data, size_t length)
{
    uint8_t calculated = calculate_checksum(data, length);
    return calculated == data[length - 1];
}

/* ============================================================================
 * VALUE ENCODING/DECODING UTILITIES
 * These functions convert between float values and integer representations
 * to maintain precision in the compact binary protocol
 * ============================================================================ */

/**
 * @brief Encode temperature for transmission (float -> int16)
 * @param temp Temperature in degrees Celsius
 * @return Encoded value (temp * 100)
 */
inline int16_t encode_temperature(float temp)
{
    return static_cast<int16_t>(temp * 100.0f);
}

/**
 * @brief Decode temperature after reception (int16 -> float)
 * @param encoded Encoded temperature value
 * @return Temperature in degrees Celsius
 */
inline float decode_temperature(int16_t encoded)
{
    return encoded / 100.0f;
}

/**
 * @brief Encode humidity for transmission (float -> uint16)
 * @param humidity Humidity percentage (0-100)
 * @return Encoded value (humidity * 100)
 */
inline uint16_t encode_humidity(float humidity)
{
    return static_cast<uint16_t>(humidity * 100.0f);
}

/**
 * @brief Decode humidity after reception (uint16 -> float)
 * @param encoded Encoded humidity value
 * @return Humidity percentage (0-100)
 */
inline float decode_humidity(uint16_t encoded)
{
    return encoded / 100.0f;
}

#endif // PROTOCOL_H

