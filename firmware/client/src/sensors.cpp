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
    prev_distance(0.0f) {}

void Sensors::setup() {
#if REAL_SENSORS_ENABLED
    // Ultrasonic sensor pins
    pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
    pinMode(PIN_ULTRASONIC_ECHO, INPUT);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    
    // Soil moisture sensor (analog input)
    pinMode(PIN_SOIL_MOISTURE, INPUT);
    analogReadResolution(12);
    
    // Initialize I2C for BH1750
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    
    // Initialize BH1750 luminosity sensor
    if (lux_sensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
        print_log("BH1750 luminosity sensor initialized\n");
    } else {
        print_log("Warning: BH1750 initialization failed\n");
    }
    
    print_log("Hardware sensors initialized\n");
#else
    print_log("Running in simulation mode\n");
#endif
}

void Sensors::read_all(float& out_humidity, float& out_distance, float& out_temperature, uint16_t& out_luminosity) {
    out_humidity = read_soil_moisture();
    out_distance = read_ultrasonic_distance();
    out_temperature = read_temperature();
    out_luminosity = read_luminosity();
    
    // Ensure values are within valid ranges
    out_humidity = constrain(out_humidity, 0.0f, 100.0f);
    out_distance = constrain(out_distance, 5.0f, 400.0f);
    out_temperature = constrain(out_temperature, -40.0f, 80.0f);
}

float Sensors::read_soil_moisture() {
#if REAL_SENSORS_ENABLED
    // Take multiple samples and average
    uint32_t sum = 0;
    for (int i = 0; i < SOIL_MOISTURE_SAMPLES; i++) {
        sum += analogRead(PIN_SOIL_MOISTURE);
        delay(10);
    }
    uint16_t avg_adc = sum / SOIL_MOISTURE_SAMPLES;
    
    // Convert ADC to percentage (inverted: lower ADC = wetter soil)
    float adc_range = static_cast<float>(SOIL_ADC_DRY - SOIL_ADC_WET);
    float humidity = 100.0f - ((avg_adc - SOIL_ADC_WET) / adc_range * 100.0f);
    
    return constrain(humidity, 0.0f, 100.0f);
#else
    return generate_simulated_value(SIM_HUMIDITY_BASE, SIM_HUMIDITY_VARIATION);
#endif
}

float Sensors::read_ultrasonic_distance() {
#if REAL_SENSORS_ENABLED
    // Send trigger pulse
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);
    
    // Measure echo duration
    long duration_us = pulseIn(PIN_ULTRASONIC_ECHO, HIGH, ULTRASONIC_TIMEOUT_US);
    
    // Convert to distance: speed of sound = 343 m/s = 0.0343 cm/µs
    // Distance = (duration * 0.0343) / 2 (round trip)
    float distance_cm = (duration_us * 0.0343f) / 2.0f;
    
    return constrain(distance_cm, 2.0f, 400.0f);
#else
    return generate_simulated_value(SIM_DISTANCE_BASE, SIM_DISTANCE_VARIATION);
#endif
}

float Sensors::read_temperature() {
#if REAL_SENSORS_ENABLED
    // TODO: Implement actual temperature sensor reading
    // For now, return a reasonable default or implement with DHT sensor
    return 25.0f;  // Default temperature
#else
    return generate_simulated_value(SIM_TEMPERATURE_BASE, SIM_TEMPERATURE_VARIATION);
#endif
}

uint16_t Sensors::read_luminosity() {
#if REAL_SENSORS_ENABLED
    float lux = lux_sensor.readLightLevel();
    if (lux < 0) {
        print_log("BH1750 read error, returning 0\n");
        return 0;
    }
    return static_cast<uint16_t>(constrain(lux, 0.0f, 65535.0f));
#else
    float lux = generate_simulated_value(SIM_LUMINOSITY_BASE, SIM_LUMINOSITY_VARIATION);
    return static_cast<uint16_t>(constrain(lux, 0.0f, 65535.0f));
#endif
}

float Sensors::generate_simulated_value(float base_value, float variation) {
    randomSeed(analogRead(0) + millis());
    float random_factor = static_cast<float>(random(-1000, 1000)) / 1000.0f;
    return base_value + (random_factor * variation);
}
