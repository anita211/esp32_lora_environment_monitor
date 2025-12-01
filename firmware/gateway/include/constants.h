#ifndef CONSTANTS_H
#define CONSTANTS_H

#define NODE_ID              23           // Max 255

#define DEBUG
// #define SIMUL_DATA
#define SIMUL_PERIOD_MS         15000
#define STATS_PERIOD_MS         60000

#define SERIAL_BAUD_RATE        115200      // Serial port baud rate

#define WIFI_ON
#define WIFI_SSID               "VIVOFIBRA-5F31"
#define WIFI_PASSWORD           "FbkfjWiv5A"
#define WIFI_TIMEOUT_MS         10000
#define SERVER_HOST             "192.168.15.7"
#define SERVER_PORT             8080
#define SERVER_ENDPOINT_DATA    "/api/sensor-data"
#define SERVER_ENDPOINT_STATS   "/api/gateway-stats"

#define LORA_PIN_CS             41          // SPI Chip Select (CS)
#define LORA_PIN_RST            42          // LoRa module reset
#define LORA_PIN_IRQ            39          // LoRa interrupt (IRQ)
#define LORA_PIN_GPIO_INT       40          // LoRa busy signal

#define LORA_FREQUENCY_MHZ      915.0       // RF frequency: 915 (Americas), 868 (EU), 433 (Asia)
#define LORA_BANDWIDTH_KHZ      125.0       // Bandwidth: 125, 250, or 500 kHz
#define LORA_SPREADING_FACTOR   9           // Spreading factor: 7-12
#define LORA_CODING_RATE        7           // Coding rate: 5-8
#define LORA_SYNC_WORD          0x12        // Network sync word (must match clients)
#define LORA_TX_POWER           10          // Transmission power in dBm
#define LORA_PREAMBLE_LENGTH    8           // Preamble symbols
#define LORA_MAX_PACKET_SIZE    256         // Maximum LoRa packet size

#define BATCH_ON
#define BATCH_SIZE              5
#define BATCH_TIMEOUT_MS        30000

#define MAX_DISTANCE_TO_BE_PRESENCE_CM 100         // Distance threshold for presence detection

#endif // CONSTANTS_H
