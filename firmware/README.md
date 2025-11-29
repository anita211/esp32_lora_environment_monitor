# LoRa Environmental Monitoring System - Firmware

This directory contains the firmware for the ESP32-S3 XIAO based LoRa monitoring system with SX1262 LoRa modules.

## 📁 Project Structure

```
firmware/
├── client/                 # Client node firmware (sensors)
│   ├── include/
│   │   ├── config.h       # Configuration parameters
│   │   └── protocol.h     # Binary message protocol
│   ├── src/
│   │   └── main.cpp       # Main client code
│   └── platformio.ini     # PlatformIO configuration
│
└── gateway/               # Gateway firmware (receiver)
    ├── include/
    │   ├── config.h       # Configuration parameters
    │   └── protocol.h     # Binary message protocol
    ├── src/
    │   └── main.cpp       # Main gateway code
    └── platformio.ini     # PlatformIO configuration
```

## 🔧 Hardware Requirements

### For Each Node (Client or Gateway)

- **Microcontroller**: Seeed XIAO ESP32-S3
- **LoRa Module**: SX1262 LoRa transceiver
- **Connection**: SPI interface

### Optional for Real Sensors (Client)

### Optional for Real Sensors (Client)

- MH-RD Soil Moisture Sensor (3.3V compatible, already implemented)
- HC-SR04 Ultrasonic Distance Sensor (already implemented)
- 3.7V LiPo battery (2000-3000mAh recommended)
- LiPo charger module

## 🔌 Wiring Diagram

### SX1262 to ESP32-S3 XIAO Connections

| SX1262 Pin | ESP32-S3 XIAO Pin | Function      |
|------------|-------------------|---------------|
| NSS        | D10               | Chip Select   |
| MOSI       | D9                | SPI MOSI      |
| MISO       | D8                | SPI MISO      |
| SCK        | D7                | SPI Clock     |
| DIO1       | D6                | IRQ           |
| RST        | D5                | Reset         |
| BUSY       | D4                | Busy Signal   |
| 3.3V       | 3V3               | Power         |
| GND        | GND               | Ground        |

**Important Notes:**
- Ensure all connections are secure
- Use short wires to minimize interference
- Add 10µF capacitor near SX1262 VCC for stability
- Check your SX1262 module's pinout (may vary by manufacturer)

## 🚀 Getting Started

### 1. Install PlatformIO

#### Option A: VS Code Extension
1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Install the PlatformIO IDE extension
3. Restart VS Code

#### Option B: Command Line
```bash
pip install platformio
```

### 2. Configure the Firmware

#### For Client Nodes

Edit `client/include/config.h`:

```cpp
// Set unique client ID (different for each node)
#define CLIENT_ID           1  // Change to 2, 3, etc. for additional nodes

// Adjust LoRa frequency for your region
#define LORA_FREQUENCY      915.0   // 915 MHz (Americas)
                                     // 868 MHz (Europe)
                                     // 433 MHz (Asia)

// Configure transmission interval
#define TX_INTERVAL_MS      60000   // 60 seconds
```

#### For Gateway

Edit `gateway/include/config.h`:

```cpp
// WiFi credentials
#define WIFI_SSID           "YourSSID"
#define WIFI_PASSWORD       "YourPassword"

// Server configuration
#define SERVER_HOST         "192.168.1.100"  // Your server IP
#define SERVER_PORT         8080
#define SERVER_ENDPOINT     "/api/sensor-data"

// Choose communication method
#define USE_HTTP            true   // HTTP POST to server
#define USE_SERIAL          true   // Output to Serial
```

### 3. Build and Upload

#### Using VS Code + PlatformIO

1. Open the `firmware/client` or `firmware/gateway` folder
2. Click the PlatformIO icon in the sidebar
3. Click "Upload" under the project tasks

#### Using Command Line

```bash
# For client
cd client
platformio run --target upload

# For gateway
cd gateway
platformio run --target upload
```

### 4. Monitor Serial Output

#### VS Code
Click "Monitor" in PlatformIO tasks

#### Command Line
```bash
platformio device monitor -b 115200
```

## 📡 System Architecture

```
┌─────────────────┐
│  Client Node 1  │ ──┐
│   (Sensors)     │   │
└─────────────────┘   │
                      │ LoRa 915MHz
┌─────────────────┐   │ (Binary Protocol)
│  Client Node 2  │ ──┤
│   (Sensors)     │   │
└─────────────────┘   │
                      │
┌─────────────────┐   │
│  Client Node N  │ ──┘
│   (Sensors)     │
└─────────────────┘

                      ↓

              ┌──────────────┐
              │   Gateway    │
              │  (Receiver)  │
              └──────────────┘
                      │
          ┌───────────┴───────────┐
          │                       │
        WiFi                   Serial
     (HTTP POST)            (USB/UART)
          │                       │
          ↓                       ↓
    ┌──────────┐           ┌──────────┐
    │  Server  │    or     │ PC/RPi   │
    │  (HTTP)  │           │ (Serial) │
    └──────────┘           └──────────┘
```

## 📊 Binary Protocol

The system uses a compact binary protocol to minimize transmission time and power consumption.

### Sensor Data Message (16 bytes)

| Offset | Size | Field       | Description                    |
|--------|------|-------------|--------------------------------|
| 0      | 1    | msg_type    | 0x01 for sensor data          |
| 1      | 1    | client_id   | Node identifier (1-255)       |
| 2      | 4    | timestamp   | Milliseconds since boot       |
| 6      | 2    | temperature | Not used (set to 0)           |
| 8      | 2    | humidity    | Soil moisture × 100 (%)       |
| 10     | 2    | distance_cm | Distance in cm (ultrasonic)   |
| 12     | 1    | battery     | Battery level (0-100%)        |
| 13     | 1    | checksum    | XOR checksum                  |
| 14     | 2    | reserved    | Reserved for future use       |

**Efficiency:** Only 16 bytes vs. ~150 bytes for JSON format

## ⚡ Power Optimization Features

### Client Node

1. **Deep Sleep Mode**
   - Wakes up periodically to read sensors
   - Configurable sleep interval (default: 60 seconds)
   - Reduces power consumption to ~10µA in sleep

2. **Adaptive Transmission**
   - Only transmits when values change significantly
   - Configurable thresholds (soil moisture: 2%, distance: 10cm)
   - Periodic heartbeat every 10 cycles

3. **Binary Protocol**
   - Minimal packet size (16 bytes)
   - Faster transmission = less radio-on time
   - Reduced power consumption

4. **Optimized LoRa Settings**
   - Spreading Factor 9 (balance between range and speed)
   - 125 kHz bandwidth
   - Current limited to 140mA

### Expected Battery Life

With a 2000mAh LiPo battery:
- **Without optimization**: ~2-3 days
- **With deep sleep**: ~2-4 weeks
- **With adaptive TX**: ~1-3 months

## 🔍 Testing & Debugging

### Test Client Transmission

1. Upload client firmware to first ESP32
2. Open serial monitor (115200 baud)
3. You should see:
   ```
   LoRa Client Node - Environmental Monitor
   Boot count: 1
   Client ID: 1
   Humidity: 62.34 %
   Distance: 75 cm
   Presence: DETECTED
   ✓ Transmission successful
   Entering deep sleep...
   ```

### Test Gateway Reception

1. Upload gateway firmware to second ESP32
2. Open serial monitor (115200 baud)
3. Turn on client node
4. Gateway should show:
   ```
   ╔═══════════════════════════════════════
   ║ Packet received (#1)
   ╠═══════════════════════════════════════
   ║ Length: 16 bytes
   ║ RSSI: -45.50 dBm
   ║ SNR: 9.25 dB
   ╚═══════════════════════════════════════
   
   ┌─ Sensor Data ──────────────────────
   │ Node ID: node-1
   │ Humidity: 62.34 %
   │ Distance: 75 cm
   │ Presence: DETECTED
   │ Battery: 95 %
   └────────────────────────────────────
   ```

### Check Signal Quality

- **RSSI (Received Signal Strength Indicator)**
  - -30 dBm: Excellent
  - -60 dBm: Good
  - -90 dBm: Weak
  - < -120 dBm: Too weak

- **SNR (Signal-to-Noise Ratio)**
  - > 10 dB: Excellent
  - 5-10 dB: Good
  - 0-5 dB: Fair
  - < 0 dB: Poor (LoRa can still decode at -20 dB!)

## 🐛 Troubleshooting

### LoRa Initialization Failed

**Symptoms:** "LoRa initialization failed, code: -2"

**Solutions:**
1. Check all wiring connections
2. Verify SPI pins in `config.h` match your wiring
3. Ensure SX1262 module is powered (3.3V)
4. Try different NSS/RST/DIO1 pins
5. Add delay after power-on: `delay(1000);` before `radio.begin()`

### No Packets Received at Gateway

**Symptoms:** Gateway shows no received packets

**Solutions:**
1. Verify both devices use same LoRa parameters (frequency, spreading factor, bandwidth)
2. Check antennas are connected
3. Reduce distance between nodes (test at < 10m first)
4. Verify client is actually transmitting (check client serial output)
5. Check for frequency mismatch (915 vs 868 vs 433 MHz)

### WiFi Connection Failed

**Symptoms:** Gateway cannot connect to WiFi

**Solutions:**
1. Verify SSID and password in `config.h`
2. Check WiFi is 2.4GHz (ESP32 doesn't support 5GHz)
3. Move closer to WiFi router
4. Check router allows new device connections
5. Try fixed IP instead of DHCP

### CRC Errors

**Symptoms:** "CRC error in received packet"

**Solutions:**
1. Reduce distance or add antenna
2. Check for interference (WiFi, Bluetooth, other LoRa devices)
3. Ensure stable power supply
4. Add decoupling capacitors

### Checksum Errors

**Symptoms:** "Checksum verification failed"

**Solutions:**
1. Ensure same `protocol.h` on client and gateway
2. Check for data corruption (improve signal quality)
3. Verify struct packing with `__attribute__((packed))`

## 📈 Performance Monitoring

### Client Metrics

- **TX success rate**: Should be > 95%
- **TX reduction**: With adaptive TX, expect 40-80% reduction
- **Boot count**: Increments each wake cycle

### Gateway Metrics

- **Success rate**: Should be > 90%
- **RSSI**: Should be better than -100 dBm
- **SNR**: Should be positive (> 0 dB)

## 🔄 Next Steps

1. **Sensors Already Implemented**
   - MH-RD soil moisture sensor (3.3V compatible)
   - HC-SR04 ultrasonic distance sensor
   - Toggle between real/simulated with `USE_REAL_SENSORS`

2. **Deploy Multiple Clients**
   - Change `CLIENT_ID` for each node
   - Place in different locations
   - Monitor from single gateway

3. **Server Already Implemented**
   - HTTP endpoint at `/api/sensor-data`
   - SQLite database storage
   - Web dashboard with real-time updates

4. **Optimize Range**
   - Add external antennas
   - Increase spreading factor (SF10-SF12)
   - Increase TX power (max 22 dBm, check regulations)

5. **Add Features**
   - Over-the-air (OTA) updates
   - Two-way communication (gateway → client commands)
   - Multiple gateway support
   - GPS location tracking

## 📚 Additional Resources

- [RadioLib Documentation](https://github.com/jgromes/RadioLib)
- [SX1262 Datasheet](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262)
- [ESP32-S3 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- [LoRa Alliance Specifications](https://lora-alliance.org/resource_hub/rp2-101-lorawan-regional-parameters-2/)
- [PlatformIO Documentation](https://docs.platformio.org/)

## 📝 License

This project is part of the CIC0236 - TELEINFORMÁTICA E REDES 2 course at UnB.

## 👥 Support

For issues or questions:
1. Check this documentation first
2. Review serial monitor output for error messages
3. Verify wiring and configuration
4. Test with minimal distance (< 10m) first

