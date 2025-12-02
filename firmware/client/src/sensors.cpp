#include "sensors.h"
#include "constants.h"
#include "utils.h"
#include <Wire.h>
#include <math.h>

Sensors* Sensors::instance = nullptr;

Sensors& Sensors::get_instance() {
    if (instance == nullptr) {
        instance = new Sensors();
    }
    return *instance;
}

Sensors::Sensors() :
    lux_sensor(BH1750_I2C_ADDRESS),
    prev_humidity(0.0f),
    prev_distance(0.0f),
    bh1750_initialized(false),
    vl53l0x_initialized(false),
    aht10_initialized(false) {
    // Constructors for VL53L0X and AHTX0 are called automatically
}

void Sensors::setup() {
#if REAL_SENSORS_ENABLED
    // Initialize I2C bus
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    delay(100);  // Give I2C time to initialize
    
    // Scan I2C bus to find connected devices
    print_log("Scanning I2C bus...\n");
    byte error, address;
    int nDevices = 0;
    bool bh1750_at_0x23 = false;
    bool bh1750_at_0x5C = false;
    
    for(address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            print_log("I2C device found at address 0x%02X\n", address);
            nDevices++;
            if (address == 0x23) bh1750_at_0x23 = true;
            if (address == 0x5C) bh1750_at_0x5C = true;
        }
    }
    if (nDevices == 0) {
        print_log("No I2C devices found! Check wiring.\n");
    } else {
        print_log("Found %d I2C device(s)\n", nDevices);
    }
    
    // Initialize BH1750 luminosity sensor
    if (bh1750_at_0x23 || bh1750_at_0x5C) {
        bh1750_initialized = lux_sensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
        if (bh1750_initialized) {
            print_log("BH1750 luminosity sensor initialized\n");
        } else {
            print_log("Warning: BH1750 found but initialization failed\n");
        }
    } else {
        print_log("Warning: BH1750 not detected on I2C bus - will return empty data\n");
        bh1750_initialized = false;
    }
    
    // Initialize VL53L0X distance sensor
    distance_sensor.setTimeout(500);
    vl53l0x_initialized = distance_sensor.init();
    if (vl53l0x_initialized) {
        distance_sensor.startContinuous();
        print_log("VL53L0X distance sensor initialized\n");
    } else {
        print_log("Warning: VL53L0X initialization failed - will return empty data\n");
    }
    
    // Initialize AHT10 temperature/humidity sensor
    aht10_initialized = aht.begin();
    if (aht10_initialized) {
        print_log("AHT10 temperature/humidity sensor initialized\n");
    } else {
        print_log("Warning: AHT10 initialization failed - will return empty data\n");
    }
    
    print_log("Hardware sensors setup complete\n");
#else
    // Initialize random seed for simulation mode using ESP32 hardware RNG
    randomSeed(esp_random());
    print_log("Running in simulation mode (random seed initialized)\n");
#endif
}

void Sensors::read_all(float& out_humidity, float& out_distance, float& out_temperature, uint16_t& out_luminosity) {
    out_humidity = read_humidity();
    out_distance = read_distance();
    out_temperature = read_temperature();
    out_luminosity = read_luminosity();
    
    // Ensure values are within valid ranges
    out_humidity = constrain(out_humidity, 0.0f, 100.0f);
    out_distance = constrain(out_distance, 0.0f, 2000.0f);  // VL53L0X max range ~2000mm
    out_temperature = constrain(out_temperature, -40.0f, 80.0f);
}

float Sensors::read_humidity() {
#if REAL_SENSORS_ENABLED
    if (!aht10_initialized) {
        return 0.0f;  // Sensor not connected, return empty value
    }
    
    sensors_event_t humidity_event, temp_event;
    if (aht.getEvent(&humidity_event, &temp_event)) {
        return constrain(humidity_event.relative_humidity, 0.0f, 100.0f);
    } else {
        print_log("AHT10 humidity read error, returning empty value\n");
        return 0.0f;  // Read error, return empty value
    }
#else
    float humidity = generate_simulated_value(SIM_HUMIDITY_BASE, SIM_HUMIDITY_VARIATION);
    print_log("[SIM] Humidity: %.1f%%\n", humidity);
    return humidity;
#endif
}

float Sensors::read_distance() {
#if REAL_SENSORS_ENABLED
    if (!vl53l0x_initialized) {
        return 0.0f;  // Sensor not connected, return empty value
    }
    
    // VL53L0X returns distance in millimeters
    uint16_t distance_mm = distance_sensor.readRangeContinuousMillimeters();
    
    if (distance_sensor.timeoutOccurred()) {
        print_log("VL53L0X timeout, returning empty value\n");
        return 0.0f;  // Timeout error, return empty value
    }
    
    // Convert mm to cm for consistency with rest of code
    float distance_cm = distance_mm / 10.0f;
    return constrain(distance_cm, 0.0f, 200.0f);  // VL53L0X effective range ~2m
#else
    float distance = generate_simulated_value(SIM_DISTANCE_BASE, SIM_DISTANCE_VARIATION);
    print_log("[SIM] Distance: %.1f cm\n", distance);
    return distance;
#endif
}

float Sensors::read_temperature() {
#if REAL_SENSORS_ENABLED
    if (!aht10_initialized) {
        return 0.0f;  // Sensor not connected, return empty value
    }
    
    sensors_event_t humidity_event, temp_event;
    if (aht.getEvent(&humidity_event, &temp_event)) {
        return constrain(temp_event.temperature, -40.0f, 80.0f);
    } else {
        print_log("AHT10 temperature read error, returning empty value\n");
        return 0.0f;  // Read error, return empty value
    }
#else
    float temperature = generate_simulated_value(SIM_TEMPERATURE_BASE, SIM_TEMPERATURE_VARIATION);
    print_log("[SIM] Temperature: %.1f C\n", temperature);
    return temperature;
#endif
}

uint16_t Sensors::read_luminosity() {
#if REAL_SENSORS_ENABLED
    if (!bh1750_initialized) {
        return 0;  // Sensor not connected, return empty value
    }
    
    float lux = lux_sensor.readLightLevel();
    if (lux < 0) {
        print_log("BH1750 read error, returning empty value\n");
        return 0;  // Read error, return empty value
    }
    return static_cast<uint16_t>(constrain(lux, 0.0f, 65535.0f));
#else
    float lux = generate_simulated_value(SIM_LUMINOSITY_BASE, SIM_LUMINOSITY_VARIATION);
    uint16_t lux_value = static_cast<uint16_t>(constrain(lux, 0.0f, 65535.0f));
    print_log("[SIM] Luminosity: %u lux\n", lux_value);
    return lux_value;
#endif
}

float Sensors::generate_simulated_value(float base_value, float variation) {
    // random() generates pseudo-random values
    // randomSeed is called once in setup via esp_random for true randomness
    float random_factor = static_cast<float>(random(-1000, 1000)) / 1000.0f;
    return base_value + (random_factor * variation);
}
