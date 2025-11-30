# 📡 Sensor Connection & Calibration Guide

## 🔌 Physical Connections

**Quick Reference:**

| Sensor | Sensor Pin | ESP32-S3 Pin | GPIO | Voltage |
|--------|------------|--------------|------|---------|
| HC-SR04 | VCC | 5V | - | 5V |
| HC-SR04 | TRIG | D0 | GPIO1 | 3.3V |
| HC-SR04 | ECHO | D1 | GPIO2 | 5V→3.3V ⚠️ |
| HC-SR04 | GND | GND | - | - |
| MH-RD | VCC | 3.3V | - | 3.3V ✓ |
| MH-RD | AO | A2 | GPIO3 | 3.3V |
| MH-RD | GND | GND | - | - |
| BH1750 | VCC | 3.3V | - | 3.3V ✓ |
| BH1750 | SDA | D3 | GPIO4 | 3.3V |
| BH1750 | SCL | D4 | GPIO5 | 3.3V |
| BH1750 | GND | GND | - | - |

### 1️⃣ **HC-SR04 (Ultrasonic Distance Sensor)**

```
HC-SR04          XIAO ESP32-S3
┌────────────┐   ┌────────────┐
│    VCC     │───│    5V      │
│    TRIG    │───│ D0 (GPIO1) │
│    ECHO    │───│ D1 (GPIO2) │ ⚠️ Use voltage divider!
│    GND     │───│    GND     │
└────────────┘   └────────────┘
```

**⚠️ IMPORTANT - Voltage Divider for ECHO:**
The HC-SR04 outputs 5V on the ECHO pin, but ESP32-S3 only accepts 3.3V!

**Option 1: Voltage Divider (Recommended)**
```
ECHO (5V) ──[ R1: 1kΩ ]──┬── D1 (GPIO2)
                          │
                     [ R2: 2kΩ ]
                          │
                         GND

Voltage at GPIO2 = 5V × (2kΩ / 3kΩ) = 3.33V ✓
```

**Option 2: Logic Level Converter (Safer)**
- Use a bidirectional 5V ↔ 3.3V level converter module

---

### 2️⃣ **MH-RD (Soil Moisture Sensor)**

```
MH-RD Module     XIAO ESP32-S3
┌────────────┐   ┌────────────┐
│    VCC     │───│   3.3V     │ ✓ Works with 3.3V
│    GND     │───│    GND     │
│ AO (Analog)│───│ A2 (GPIO3) │ ← ADC Pin
│ DO (Digital)│   │ (not used) │
└────────────┘   └────────────┘
```

**Notes:**
- ✓ **VCC at 3.3V** - This sensor works with 3.3V!
- Use only the **AO (Analog Out)** output
- The DO (Digital) output is not needed
- The sensor works **inverted**: lower ADC value = wetter soil
- Pin A2 (GPIO3) does not conflict with HC-SR04

---

### 3️⃣ **BH1750 (Luminosity Sensor)**

```
BH1750 Module    XIAO ESP32-S3
┌────────────┐   ┌────────────┐
│    VCC     │───│   3.3V     │ ✓
│    GND     │───│    GND     │
│    SDA     │───│ D3 (GPIO4) │ ← I2C Data
│    SCL     │───│ D4 (GPIO5) │ ← I2C Clock
│   ADDR     │───│ GND (0x23) │ ← Optional: VCC for 0x5C
└────────────┘   └────────────┘
```

**Notes:**
- ✓ **3.3V compatible** - Works natively with ESP32
- **I2C Address:** 0x23 (default, ADDR→GND) or 0x5C (ADDR→VCC)
- Measurement range: 1 - 65535 lux
- Automatically adjusts measurement time based on light level

**I2C Configuration in `config.h`:**
```cpp
#define PIN_I2C_SDA        4           // GPIO4 / D3
#define PIN_I2C_SCL        5           // GPIO5 / D4
#define BH1750_I2C_ADDRESS 0x23        // Default address
```

---

## 🔧 MH-RD Sensor Calibration

The MH-RD sensor needs to be **calibrated** for your specific soil!

### Step 1: Measure "Dry" Value
```cpp
// In the code, temporarily:
#define SOIL_ADC_DRY 4095  // Start with this value
```

1. Leave the sensor **completely dry** in open air for 30 minutes
2. Compile and run the code
3. Note the ADC value shown in Serial Monitor
4. Update `SOIL_ADC_DRY` with this value

### Step 2: Measure "Wet" Value
```cpp
// In the code:
#define SOIL_ADC_WET 1500  // Adjust this value
```

1. Place the sensor in **very wet soil** or water
2. Note the ADC value shown
3. Update `SOIL_ADC_WET` with this value

### Real Example:
```
Sensor in air (dry):     ADC = 4095  → 0% moisture
Sensor in water:         ADC = 800   → 100% moisture
Sensor in moist soil:    ADC = 1500  → ~60% moisture
```

**Update in `config.h`:**
```cpp
#define SOIL_ADC_DRY 4095   // Your dry value
#define SOIL_ADC_WET 800    // Your wet value
```

---

## ⚙️ Code Configuration

### To Use Real Sensors:
In the `include/config.h` file, make sure:
```cpp
#define REAL_SENSORS_ENABLED true  // ← true for real sensors
```

### To Return to Simulation (Testing):
```cpp
#define REAL_SENSORS_ENABLED false  // ← false for simulation
```

---

## 🧪 Testing the Sensors

### 1. Compile and Upload:
```bash
cd firmware/client
platformio run --target upload
platformio device monitor
```

### 2. Check Serial Monitor Output:

**Initialization:**
```
Initializing sensors...
✓ HC-SR04 initialized (TRIG: GPIO1, ECHO: GPIO2)
✓ MH-RD Moisture sensor initialized (ADC: GPIO3)
✓ BH1750 luminosity sensor initialized (I2C: 0x23)
Test readings: Moisture=45.2%, Distance=125.3cm, Lux=850
Sensors ready
```

**During Operation:**
```
--- Measurement Cycle ---
[SENSORS] MH-RD ADC: 2850, Humidity: 52.3%
[SENSORS] HC-SR04: 85.2 cm
[SENSORS] BH1750: 1250 lux
Moisture: 52.30 %
Distance: 85.20 cm
Luminosity: 1250 lux
Presence: DETECTED  ← Object < 100cm detected!
```

---

## 🐛 Troubleshooting

### HC-SR04 Always Returns 400cm (Out of Range)
✅ **Solutions:**
- Check TRIG and ECHO connections
- Use voltage divider on ECHO pin
- Ensure sensor has 5V power supply
- Test with an object ~30cm from the sensor

### MH-RD Always Shows 0% or 100%
✅ **Solutions:**
- Recalibrate DRY and WET values
- Verify ADC pin is correct (A2 = GPIO3)
- Test with sensor at different moisture levels

### "Out of range" on HC-SR04
✅ **Cause:** No object detected within 4 meters
- Place a solid object ~50cm from the sensor

### Unstable Readings
✅ **Solutions:**
- Increase `SOIL_MOISTURE_SAMPLES` to 20 (slower but more stable)
- Use short, shielded wires
- Add 100µF capacitor between sensor VCC and GND

### BH1750 Not Detected
✅ **Solutions:**
- Check I2C wiring (SDA → GPIO4, SCL → GPIO5)
- Verify I2C address (0x23 with ADDR→GND, 0x5C with ADDR→VCC)
- Use I2C scanner sketch to find device
- Ensure 3.3V power supply

### BH1750 Shows 0 or -1 lux
✅ **Solutions:**
- Sensor may be in very dark environment (normal if < 1 lux)
- Check if sensor is initialized correctly in Serial Monitor
- Verify I2C pull-up resistors (usually built into module)

---

## 📊 Data Interpretation

### Soil Moisture Sensor (MH-RD)
| ADC Value | Moisture % | Condition |
|-----------|------------|-----------|
| 4095 | 0% | Dry (air) |
| 3000-3500 | 20-40% | Dry soil |
| 2000-2500 | 50-70% | Moist soil |
| 800-1500 | 80-100% | Wet soil |

### Ultrasonic Sensor (HC-SR04)
| Distance | Presence | Use Case |
|----------|----------|----------|
| < 50cm | ✓ Detected | Object very close |
| 50-100cm | ✓ Detected | Within threshold |
| 100-200cm | ✗ Not detected | Outside threshold |
| > 200cm | ✗ Not detected | Far away or no object |

### Luminosity Sensor (BH1750)
| Lux Value | Condition | Environment |
|-----------|-----------|-------------|
| 0-50 | Very dark | Night, closed room |
| 50-200 | Dim | Indoor, cloudy day |
| 200-1000 | Normal indoor | Office, home |
| 1000-10000 | Bright | Near window, shade |
| 10000-50000 | Very bright | Direct sunlight |
| > 50000 | Extremely bright | Full sun exposure |

---

## 🔋 Power Consumption

**Active Mode (with sensors):**
- ESP32-S3: ~240mA
- HC-SR04: ~15mA (during measurement, 2ms)
- MH-RD: ~5mA
- BH1750: ~0.12mA (measurement), ~0.01mA (sleep)
- **Total: ~260mA for ~3 seconds**

**Deep Sleep Mode:**
- ESP32-S3: ~10µA
- Sensors automatically powered off
- **Battery life: weeks to months**

---

## 📝 Checklist Before Powering On

- [ ] HC-SR04 has **voltage divider** on ECHO
- [ ] MH-RD connected to A2 pin (GPIO3)
- [ ] BH1750 connected via I2C (SDA→GPIO4, SCL→GPIO5)
- [ ] HC-SR04 powered with **5V**
- [ ] MH-RD and BH1750 powered with **3.3V**
- [ ] `REAL_SENSORS_ENABLED` = `true` in config.h
- [ ] DRY and WET values calibrated for soil moisture
- [ ] Serial Monitor open for debugging

---

## 🎯 Next Steps

1. **Individual Test:** Test each sensor separately first
2. **Calibration:** Calibrate the MH-RD for your soil
3. **Integration:** Test both sensors together
4. **Deep Sleep:** Enable deep sleep after confirming everything works
5. **Deploy:** Put into production!

Good luck! 🚀
