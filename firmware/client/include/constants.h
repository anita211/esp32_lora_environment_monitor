#ifndef CONSTANTS_H
#define CONSTANTS_H

#define NODE_ID              23           // Max 255

#define DEBUG
#define SIMUL_DATA
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

#define LORA_PIN_MOSI           9           // SPI Master Out Slave In
#define LORA_PIN_MISO           8           // SPI Master In Slave Out
#define LORA_PIN_SCK            7           // SPI Clock
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

#define PIN_I2C_SDA             4           // I2C Data pin
#define PIN_I2C_SCL             5           // I2C Clock pin
#define BH1750_I2C_ADDRESS      0x23        // BH1750 default I2C address (0x23 or 0x5C)
#define VL53L0X_I2C_ADDRESS     0x29        // VL53L0X default I2C address
#define AHT10_I2C_ADDRESS       0x38        // AHT10 default I2C address

#define SIM_HUMIDITY_BASE       55.0        // Base humidity value (%)
#define SIM_HUMIDITY_VARIATION  35.0        // +/- variation range
#define SIM_DISTANCE_BASE       150.0       // Base distance value (cm)
#define SIM_DISTANCE_VARIATION  120.0       // +/- variation range
#define SIM_TEMPERATURE_BASE    25.0        // Base temperature value (°C)
#define SIM_TEMPERATURE_VARIATION 10.0      // +/- variation range
#define SIM_LUMINOSITY_BASE     500.0       // Base luminosity value (lux)
#define SIM_LUMINOSITY_VARIATION 400.0      // +/- variation range

#define TX_INTERVAL_MS          30000       // Time between transmissions (milliseconds)
#define TX_MAX_RETRIES          3           // Max retry attempts on TX failure

#define ADAPTIVE_TX_ENABLED     false       // Enable/disable adaptive transmission
#define HUMIDITY_CHANGE_THRESHOLD   2.0     // Min humidity change (%) to trigger TX
#define DISTANCE_CHANGE_THRESHOLD   10.0    // Min distance change (cm) to trigger TX

#define DEEP_SLEEP_ENABLED      true        // Use deep sleep between transmissions
#define DEEP_SLEEP_TIME_US      (TX_INTERVAL_MS * 1000ULL)  // Sleep duration in microseconds

#define REAL_SENSORS_ENABLED    false       // true = real hardware, false = simulation

#endif // CONSTANTS_H
