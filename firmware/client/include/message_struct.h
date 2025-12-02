#ifndef MESSAGE_STRUCT_H
#define MESSAGE_STRUCT_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    MSG_TYPE_SENSOR_DATA    = 0x01,     // Regular sensor data transmission
    MSG_TYPE_HEARTBEAT      = 0x02,     // Keep-alive / status message
    MSG_TYPE_ALERT          = 0x03,     // Alert/alarm notification
    MSG_TYPE_ACK            = 0xAA,     // Acknowledgment from gateway
} MessageType;

typedef enum {
    STATUS_OK               = 0x00,     // All systems normal
    STATUS_LOW_BATTERY      = 0x01,     // Battery below threshold
    STATUS_SENSOR_ERROR     = 0x02,     // Sensor read failure
    STATUS_LORA_ERROR       = 0x04,     // LoRa communication issue
} NodeStatus;

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

struct __attribute__((packed)) HeartbeatMessage {
    uint8_t     msg_type;       // MSG_TYPE_HEARTBEAT (0x02)
    uint8_t     client_id;      // Node identifier
    uint32_t    timestamp;      // Milliseconds since boot
    uint8_t     status;         // NodeStatus flags
    uint8_t     checksum;       // XOR checksum
};

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

#endif // MESSAGE_STRUCT_H

