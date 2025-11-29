#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define MSG_TYPE_SENSOR_DATA 0x01
#define MSG_TYPE_HEARTBEAT 0x02
#define MSG_TYPE_ALERT 0x03
#define MSG_TYPE_ACK 0xAA

struct __attribute__((packed)) SensorDataMessage {
    uint8_t msg_type;
    uint8_t client_id;
    uint32_t timestamp;
    int16_t temperature;
    uint16_t humidity;
    uint16_t distance_cm;
    uint8_t battery;
    uint16_t reserved;
    uint8_t checksum;
};

struct __attribute__((packed)) HeartbeatMessage {
    uint8_t msg_type;
    uint8_t client_id;
    uint32_t timestamp;
    uint8_t status;
    uint8_t checksum;
};

struct __attribute__((packed)) AlertMessage {
    uint8_t msg_type;
    uint8_t client_id;
    uint32_t timestamp;
    uint8_t alert_code;
    int16_t alert_value;
    uint8_t severity;
    uint8_t reserved;
    uint8_t checksum;
};

#define STATUS_OK 0x00
#define STATUS_LOW_BATTERY 0x01
#define STATUS_SENSOR_ERROR 0x02
#define STATUS_LORA_ERROR 0x04
#define ALERT_TEMP_HIGH 0x10
#define ALERT_TEMP_LOW 0x11
#define ALERT_HUMIDITY_HIGH 0x20
#define ALERT_HUMIDITY_LOW 0x21
#define ALERT_DISTANCE_LOW 0x30

inline uint8_t calculate_checksum(const uint8_t* data, size_t length)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < length - 1; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

inline bool verify_checksum(const uint8_t* data, size_t length)
{
    uint8_t calculated = calculate_checksum(data, length);
    return calculated == data[length - 1];
}

inline int16_t encode_temperature(float temp)
{
    return (int16_t)(temp * 100);
}

inline float decode_temperature(int16_t temp)
{
    return temp / 100.0f;
}

inline uint16_t encode_humidity(float humid)
{
    return (uint16_t)(humid * 100);
}

inline float decode_humidity(uint16_t humid)
{
    return humid / 100.0f;
}

#endif
