# 🌿 ESP32 LoRa Environment Monitor

A wireless environmental monitoring system using ESP32-S3 XIAO microcontrollers with SX1262 LoRa modules. Sensor nodes collect soil moisture and distance data, transmitting it via LoRa to a gateway that forwards data to a web server with real-time dashboard.

## 📁 Project Structure

```
esp32_lora_environment_monitor/
├── README.md
├── firmware/
│   ├── client/                     # Sensor node firmware
│   │   ├── include/
│   │   │   ├── config.h            # Node configuration
│   │   │   └── protocol.h          # Binary protocol definitions
│   │   ├── src/
│   │   │   └── main.cpp            # Main sensor node code
│   │   ├── platformio.ini
│   │   └── SENSORS.md              # Sensor wiring & calibration guide
│   │
│   └── gateway/                    # Gateway firmware
│       ├── include/
│       │   ├── config.h            # Gateway configuration
│       │   └── protocol.h          # Binary protocol definitions
│       ├── src/
│       │   └── main.cpp            # Main gateway code
│       └── platformio.ini
│
└── server/
    ├── server.py                   # HTTP server with SQLite database
    ├── mock_server.py              # Mock server for testing dashboard
    ├── dashboard.html              # Real-time monitoring dashboard
    └── style.css                   # Dashboard styling
```

## 🔧 Hardware Requirements

### Sensor Node (Client)
- **Microcontroller**: Seeed XIAO ESP32-S3
- **LoRa Module**: SX1262 transceiver
- **Sensors**:
  - MH-RD Soil Moisture Sensor (3.3V compatible)
  - HC-SR04 Ultrasonic Distance Sensor (requires voltage divider)
- **Power**: 3.7V LiPo battery (2000-3000mAh recommended)

### Gateway
- **Microcontroller**: Seeed XIAO ESP32-S3
- **LoRa Module**: SX1262 transceiver
- **Connection**: WiFi to local network

## 🔌 Wiring Diagrams

### SX1262 LoRa Module (Both Client & Gateway)

| SX1262 Pin | ESP32-S3 GPIO | Function |
|------------|---------------|----------|
| NSS        | GPIO41        | SPI Chip Select |
| MOSI       | GPIO9         | SPI MOSI |
| MISO       | GPIO8         | SPI MISO |
| SCK        | GPIO7         | SPI Clock |
| DIO1       | GPIO39        | Interrupt |
| RST        | GPIO42        | Reset |
| BUSY       | GPIO40        | Busy Signal |
| 3.3V       | 3V3           | Power |
| GND        | GND           | Ground |

### Sensors (Client Only)

| Sensor | Sensor Pin | ESP32-S3 | Notes |
|--------|------------|----------|-------|
| HC-SR04 | VCC | 5V | Requires 5V |
| HC-SR04 | TRIG | GPIO1 (D0) | 3.3V logic |
| HC-SR04 | ECHO | GPIO2 (D1) | ⚠️ Use voltage divider! |
| HC-SR04 | GND | GND | |
| MH-RD | VCC | 3.3V | Works at 3.3V |
| MH-RD | AO | GPIO3 (A2) | Analog output |
| MH-RD | GND | GND | |

> ⚠️ **Important**: The HC-SR04 ECHO pin outputs 5V but ESP32-S3 is 3.3V only! Use a voltage divider (1kΩ + 2kΩ) or level shifter. See `firmware/client/SENSORS.md` for details.

## 🚀 Getting Started

### 1. Install PlatformIO

```bash
# VS Code Extension (recommended)
# Install "PlatformIO IDE" from VS Code marketplace

# Or command line
pip install platformio
```

### 2. Configure Client Node

Edit `firmware/client/include/config.h`:

```cpp
// Node identification
#define NODE_ID                 1           // Unique ID (1-255)

// LoRa settings (must match gateway)
#define LORA_FREQUENCY_MHZ      915.0       // 915 (Americas), 868 (EU), 433 (Asia)
#define LORA_SPREADING_FACTOR   9           // 7-12 (higher = longer range)
#define LORA_TX_POWER_DBM       20          // Transmit power (2-20 dBm)

// Transmission interval
#define TX_INTERVAL_MS          30000       // 30 seconds

// Sensors
#define REAL_SENSORS_ENABLED    false       // true for real hardware

// Power saving
#define DEEP_SLEEP_ENABLED      true        // Enable deep sleep
#define ADAPTIVE_TX_ENABLED     false       // Skip TX if values unchanged
```

### 3. Configure Gateway

Edit `firmware/gateway/include/config.h`:

```cpp
// WiFi credentials
#define WIFI_SSID           "YourSSID"
#define WIFI_PASSWORD       "YourPassword"

// Server configuration
#define SERVER_HOST         "192.168.1.100"  // Server IP address
#define SERVER_PORT         8080
#define SERVER_ENDPOINT     "/api/sensor-data"

// LoRa settings (must match clients)
#define LORA_FREQUENCY      915.0
#define LORA_SPREADING      9
```

### 4. Build and Upload

```bash
# Client node
cd firmware/client
platformio run --target upload
platformio device monitor

# Gateway
cd firmware/gateway
platformio run --target upload
platformio device monitor
```

### 5. Start the Server

```bash
cd server
python3 server.py
```

Server runs on `http://0.0.0.0:8080`

### 6. View Dashboard

Open `http://<server-ip>:8080/dashboard.html` in your browser.

For testing without hardware:
```bash
cd server
python3 mock_server.py
# Open http://127.0.0.1:8888/dashboard.html
```

## 📡 System Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  Sensor Node 1  │     │  Sensor Node 2  │     │  Sensor Node N  │
│  (Client)       │     │  (Client)       │     │  (Client)       │
│  ┌───────────┐  │     │  ┌───────────┐  │     │  ┌───────────┐  │
│  │ MH-RD     │  │     │  │ MH-RD     │  │     │  │ MH-RD     │  │
│  │ HC-SR04   │  │     │  │ HC-SR04   │  │     │  │ HC-SR04   │  │
│  └───────────┘  │     │  └───────────┘  │     │  └───────────┘  │
└────────┬────────┘     └────────┬────────┘     └────────┬────────┘
         │                       │                       │
         └───────────────────────┼───────────────────────┘
                                 │
                          LoRa 915 MHz
                       (Binary Protocol)
                                 │
                                 ▼
                    ┌────────────────────────┐
                    │       Gateway          │
                    │   ESP32-S3 + SX1262    │
                    └────────────┬───────────┘
                                 │
                              WiFi
                           (HTTP POST)
                                 │
                                 ▼
                    ┌────────────────────────┐
                    │     Python Server      │
                    │  ┌──────────────────┐  │
                    │  │  SQLite Database │  │
                    │  └──────────────────┘  │
                    │  ┌──────────────────┐  │
                    │  │  Web Dashboard   │  │
                    │  └──────────────────┘  │
                    └────────────────────────┘
```

## 📊 Binary Protocol

Compact 16-byte message format for efficient LoRa transmission:

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | msg_type | Message type (0x01 = sensor data) |
| 1 | 1 | client_id | Node ID (1-255) |
| 2 | 4 | timestamp | Milliseconds since boot |
| 6 | 2 | temperature | Reserved (not used) |
| 8 | 2 | humidity | Soil moisture × 100 |
| 10 | 2 | distance_cm | Distance in cm |
| 12 | 1 | battery | Battery level (0-100%) |
| 13 | 2 | reserved | Reserved for future use |
| 15 | 1 | checksum | XOR checksum |

**Message Types:**
- `0x01` - Sensor Data
- `0x02` - Heartbeat
- `0x03` - Alert
- `0xAA` - Acknowledgment

## ⚡ Power Optimization

### Features
- **Deep Sleep**: ~10µA consumption between readings
- **Adaptive TX**: Skip transmission if values unchanged
- **Compact Protocol**: 16 bytes vs ~150 bytes JSON

### Expected Battery Life (2000mAh LiPo)
| Mode | Battery Life |
|------|--------------|
| No optimization | 2-3 days |
| Deep sleep only | 2-4 weeks |
| Deep sleep + adaptive TX | 1-3 months |

### Configuration
```cpp
// In config.h
#define DEEP_SLEEP_ENABLED      true    // Enable deep sleep
#define ADAPTIVE_TX_ENABLED     true    // Skip TX if unchanged
#define HUMIDITY_CHANGE_THRESHOLD   2.0 // Min change to trigger TX
#define DISTANCE_CHANGE_THRESHOLD   10.0
```

## 📈 Dashboard Features

- **Real-time data**: Updates every 5 seconds
- **Active nodes count**: Shows number of reporting sensors
- **Connection status**: Online/Offline indicator
- **Sensor readings**:
  - 💧 Soil moisture (%) with color-coded status
  - 📏 Distance (cm)
  - 🔋 Battery level with warnings
  - 📶 RSSI signal strength
  - 📊 SNR (Signal-to-Noise Ratio)
- **Alerts panel**: Recent system alerts

## 🐛 Troubleshooting

### LoRa Initialization Failed
```
[LORA] FAILED to initialize (error -2)
```
- Check SPI wiring connections
- Verify pins in `config.h` match your wiring
- Add 10µF capacitor near SX1262 VCC
- Try `delay(1000)` before `radio.begin()`

### No Packets Received at Gateway
- Verify same LoRa parameters on both devices
- Check antennas are connected
- Test at short range first (< 10m)
- Confirm client is transmitting (check serial output)

### WiFi Connection Failed
- Verify SSID/password in gateway `config.h`
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
- Check router allows new connections

### Sensor Reading Issues
- See `firmware/client/SENSORS.md` for calibration guide
- HC-SR04 returns 400cm: Check ECHO voltage divider
- MH-RD always 0% or 100%: Recalibrate ADC values

## 📊 Signal Quality Reference

### RSSI (Received Signal Strength)
| RSSI | Quality |
|------|---------|
| > -60 dBm | Excellent |
| -60 to -80 dBm | Good |
| -80 to -100 dBm | Fair |
| < -100 dBm | Weak |

### SNR (Signal-to-Noise Ratio)
| SNR | Quality |
|-----|---------|
| > 10 dB | Excellent |
| 5 to 10 dB | Good |
| 0 to 5 dB | Fair |
| < 0 dB | Poor (LoRa can decode down to -20dB) |

## 📚 Additional Resources

- [RadioLib Documentation](https://github.com/jgromes/RadioLib)
- [SX1262 Datasheet](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262)
- [ESP32-S3 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [PlatformIO Documentation](https://docs.platformio.org/)

## 📝 License

This project is part of the CIC0236 - TELEINFORMÁTICA E REDES 2 course at UnB.

