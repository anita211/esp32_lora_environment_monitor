# 📡 Sensor Connection & Calibration Guide

## 🔌 Physical Connections

**Quick Reference:**

| Sensor  | Sensor Pin | ESP32-S3 Pin | GPIO  | Voltage |
| ------- | ---------- | ------------ | ----- | ------- |
| BH1750  | VCC        | 3.3V         | -     | 3.3V ✓  |
| BH1750  | SDA        | D3           | GPIO4 | 3.3V    |
| BH1750  | SCL        | D4           | GPIO5 | 3.3V    |
| BH1750  | GND        | GND          | -     | -       |
| VL53L0X | VCC        | 3.3V         | -     | 3.3V ✓  |
| VL53L0X | SDA        | D3           | GPIO4 | 3.3V    |
| VL53L0X | SCL        | D4           | GPIO5 | 3.3V    |
| VL53L0X | GND        | GND          | -     | -       |
| AHT10   | VCC        | 3.3V         | -     | 3.3V ✓  |
| AHT10   | SDA        | D3           | GPIO4 | 3.3V    |
| AHT10   | SCL        | D4           | GPIO5 | 3.3V    |
| AHT10   | GND        | GND          | -     | -       |

### 1️⃣ **BH1750 (Luminosity Sensor)**

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
- High resolution mode: 1 lux resolution

**I2C Configuration in `constants.h`:**

```cpp
#define PIN_I2C_SDA        4           // GPIO4 / D3
#define PIN_I2C_SCL        5           // GPIO5 / D4
#define BH1750_I2C_ADDRESS 0x23        // Default address
```

---

### 2️⃣ **VL53L0X / VL53L1X (ToF Distance Sensor)**

```
VL53L0X Module   XIAO ESP32-S3
┌────────────┐   ┌────────────┐
│    VCC     │───│   3.3V     │ ✓
│    GND     │───│    GND     │
│    SDA     │───│ D3 (GPIO4) │ ← I2C Data
│    SCL     │───│ D4 (GPIO5) │ ← I2C Clock
│   XSHUT    │───│ (optional) │ ← Shutdown (not used)
│   GPIO1    │───│ (optional) │ ← Interrupt (not used)
└────────────┘   └────────────┘
```

**Notes:**

- ✓ **3.3V compatible** - Works natively with ESP32
- **I2C Address:** 0x29 (default)
- **Technology:** Time-of-Flight (ToF) laser ranging
- **Range:** 30mm to 2000mm (VL53L0X) or up to 4000mm (VL53L1X)
- **Accuracy:** ±3% at typical distances
- Ambient light independent
- Fast measurement: up to 50Hz

**VL53L0X vs VL53L1X:**

- VL53L0X: Max range ~2m, good for most applications
- VL53L1X: Max range ~4m, better ambient light immunity

---

### 3️⃣ **AHT10 (Temperature & Humidity Sensor)**

```
AHT10 Module     XIAO ESP32-S3
┌────────────┐   ┌────────────┐
│    VCC     │───│   3.3V     │ ✓
│    GND     │───│    GND     │
│    SDA     │───│ D3 (GPIO4) │ ← I2C Data
│    SCL     │───│ D4 (GPIO5) │ ← I2C Clock
└────────────┘   └────────────┘
```

**Notes:**

- ✓ **3.3V compatible** - Works natively with ESP32
- **I2C Address:** 0x38 (fixed)
- **Temperature Range:** -40°C to +85°C
- **Temperature Accuracy:** ±0.3°C (typical)
- **Humidity Range:** 0% to 100% RH
- **Humidity Accuracy:** ±2% RH (typical)
- Low power consumption
- Fast response time

---

## 🧪 I2C Bus Wiring

All three sensors share the same I2C bus:

```
ESP32-S3 GPIO4 (SDA) ───┬─── BH1750 SDA
                        ├─── VL53L0X SDA
                        └─── AHT10 SDA

ESP32-S3 GPIO5 (SCL) ───┬─── BH1750 SCL
                        ├─── VL53L0X SCL
                        └─── AHT10 SCL

3.3V ───────────────────┬─── BH1750 VCC
                        ├─── VL53L0X VCC
                        └─── AHT10 VCC

GND ────────────────────┬─── BH1750 GND
                        ├─── VL53L0X GND
                        └─── AHT10 GND
```

**I2C Addresses:**

- BH1750: 0x23 (or 0x5C if ADDR connected to VCC)
- VL53L0X: 0x29 (default)
- AHT10: 0x38 (fixed)

**Notes:**

- ✓ No address conflicts - all sensors have different I2C addresses
- ✓ All sensors are 3.3V compatible
- ✓ No pull-up resistors needed (usually built-in on modules)
- If you have I2C communication issues, add external 4.7kΩ pull-ups on SDA and SCL

---

## 🔧 No Calibration Required!

Unlike analog sensors, all three I2C sensors are **factory calibrated**:

- **BH1750**: Calibrated light sensor, direct lux readout

### VL53L0X Distance Sensor

| Distance  | Presence       | Use Case              |
| --------- | -------------- | --------------------- |
| < 50cm    | ✓ Detected     | Object very close     |
| 50-100cm  | ✓ Detected     | Within threshold      |
| 100-200cm | ✗ Not detected | Outside threshold     |
| > 200cm   | ✗ Not detected | Far away or no object |

### BH1750 Luminosity Sensor

| Lux Value   | Condition        | Environment        |
| ----------- | ---------------- | ------------------ |
| 0-50        | Very dark        | Night, closed room |
| 50-200      | Dim              | Indoor, cloudy day |
| 200-1000    | Normal indoor    | Office, home       |
| 1000-10000  | Bright           | Near window, shade |
| 10000-50000 | Very bright      | Direct sunlight    |
| > 50000     | Extremely bright | Full sun exposure  |

---

## 🔋 Power Consumption

**Active Mode (with sensors):**
- ESP32-S3: ~240mA
- VL53L0X: ~19mA (during measurement)
- AHT10: ~0.55mA (during measurement)
- BH1750: ~0.12mA (measurement), ~0.01mA (sleep)
- **Total: ~260mA for ~3 seconds**

**Deep Sleep Mode:**
- ESP32-S3: ~10µA
- Sensors automatically powered off via I2C disable
- **Battery life: weeks to months**

---

## 📝 Checklist Before Powering On

- [ ] All sensors connected via I2C (SDA→GPIO4, SCL→GPIO5)
- [ ] All sensors powered with **3.3V**
- [ ] Common GND connection
- [ ] `REAL_SENSORS_ENABLED` = `true` in constants.h
- [ ] Libraries installed via PlatformIO
- [ ] Serial Monitor open for debugging

---

## 🎯 Next Steps

1. **Install Libraries:** PlatformIO will auto-install on first build
2. **Individual Test:** Verify each sensor I2C address with scanner
3. **Integration:** Test all sensors together
4. **Deploy:** Set REAL_SENSORS_ENABLED=true and upload

**Happy sensing! 🎉**
- Ensure target is between 3cm and 200cm
- Check that sensor lens is clean
- Avoid highly reflective or transparent surfaces
- Ensure sensor is stable and not moving

### AHT10 Returns Error Values
✅ **Solutions:**
- Wait 500ms after power-up before first reading
- Ensure sensor is not exposed to condensation
- Check I2C communication with other sensors first
- Try power cycling the sensor

### BH1750 Returns 0 or 65535 lux
✅ **Solutions:**
- Very dark: < 1 lux may read as 0 (normal)
- Very bright: > 65535 lux will saturate (normal in direct sunlight)
- Check sensor orientation (light sensor facing up)
- Verify I2C communication

### Unstable Readings
✅ **Solutions:**
- Use short, shielded I2C wires (< 20cm recommended)
- Add 100µF capacitor between sensor VCC and GND
- Reduce I2C clock speed if necessary
- Ensure stable power supply

---

## 📊 Data Interpretation

### AHT10 Temperature & Humidity

| Temperature | Condition |
|------------|-----------|
| < 10°C | Cold |
| 10-20°C | Cool |
| 20-25°C | Comfortable |
| 25-30°C | Warm |
| > 30°C | Hot |

| Humidity | Condition |
|----------|-----------|
| < 30% | Very dry |
| 30-40% | Dry |
| 40-60% | Comfortable |
| 60-80% | Humid |
| > 80% | Very humid |

### VL53L0X Distance Sensor

| Distance  | Presence       | Use Case              |
| --------- | -------------- | --------------------- |
| < 50cm    | ✓ Detected     | Object very close     |
| 50-100cm  | ✓ Detected     | Within threshold      |
| 100-200cm | ✗ Not detected | Outside threshold     |
| > 200cm   | ✗ Not detected | Far away or no object |

### BH1750 Luminosity Sensor

| Lux Value   | Condition        | Environment        |
| ----------- | ---------------- | ------------------ |
| 0-50        | Very dark        | Night, closed room |
| 50-200      | Dim              | Indoor, cloudy day |
| 200-1000    | Normal indoor    | Office, home       |
| 1000-10000  | Bright           | Near window, shade |
| 10000-50000 | Very bright      | Direct sunlight    |
| > 50000     | Extremely bright | Full sun exposure  |

---

## 🔋 Power Consumption

**Active Mode (with sensors):**
- ESP32-S3: ~240mA
- VL53L0X: ~19mA (during measurement)
- AHT10: ~0.55mA (during measurement)
- BH1750: ~0.12mA (measurement), ~0.01mA (sleep)
- **Total: ~260mA for ~3 seconds**

**Deep Sleep Mode:**
- ESP32-S3: ~10µA
- Sensors automatically powered off via I2C disable
- **Battery life: weeks to months**

---

## 📝 Checklist Before Powering On

- [ ] All sensors connected via I2C (SDA→GPIO4, SCL→GPIO5)
- [ ] All sensors powered with **3.3V**
- [ ] Common GND connection
- [ ] `REAL_SENSORS_ENABLED` = `true` in constants.h
- [ ] Libraries installed via PlatformIO
- [ ] Serial Monitor open for debugging

---

## 🎯 Next Steps

1. **Install Libraries:** PlatformIO will auto-install on first build
2. **Individual Test:** Verify each sensor I2C address with scanner
3. **Integration:** Test all sensors together
4. **Deploy:** Set REAL_SENSORS_ENABLED=true and upload

**Happy sensing! 🎉**
