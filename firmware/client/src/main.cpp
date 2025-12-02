/**
 * @file main.cpp
 * @brief ESP32 LoRa Environment Monitor - Sensor Node (Client)
 * 
 * This firmware reads environmental sensors (soil moisture, ultrasonic distance,
 * temperature, and luminosity) and transmits data via LoRa to a gateway node.
 * 
 * Hardware:
 *   - Seeed XIAO ESP32-S3
 *   - SX1262 LoRa module
 *   - MH-RD soil moisture sensor
 *   - HC-SR04 ultrasonic distance sensor
 *   - BH1750 luminosity sensor (I2C)
 */

#include <Arduino.h>
#include <RadioLib.h>
#include <Wire.h>
#include <BH1750.h>
#include <math.h>
#include "constants.h"
#include "message_struct.h"
#include "utils.h"

/* ============================================================================
 * GLOBAL STATE
 * ============================================================================ */

// LoRa radio instance
static SX1262 g_radio = new Module(LORA_PIN_CS, LORA_PIN_IRQ, LORA_PIN_RST, LORA_PIN_GPIO_INT);

// BH1750 luminosity sensor instance
static BH1750 g_lux_sensor(BH1750_I2C_ADDRESS);

// Previous sensor readings (for adaptive transmission)
static float g_prev_humidity = 0.0f;
static float g_prev_distance = 0.0f;

// Transmission statistics
static struct {
    uint32_t total;
    uint32_t success;
    uint32_t failed;
    uint32_t skipped;
} g_tx_stats = {0, 0, 0, 0};

// Hardware state
static bool g_lora_ready = false;

// Persisted across deep sleep (stored in RTC memory)
RTC_DATA_ATTR static uint32_t g_boot_count = 0;

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================ */

// Initialization
static void init_lora_radio();
static void init_sensors();

// Sensor reading
static float read_soil_moisture();
static float read_ultrasonic_distance();
static float read_temperature();
static uint16_t read_luminosity();
static void  read_all_sensors(float& out_humidity, float& out_distance, float& out_temperature, uint16_t& out_luminosity);

// Transmission logic
static bool should_transmit(float humidity, float distance);
static bool transmit_sensor_data(float humidity, float distance, float temperature, uint16_t luminosity);

// Power management
static void enter_deep_sleep();

// Utilities
static void print_statistics();
static float generate_simulated_value(float base_value, float variation);

/* ============================================================================
 * SETUP
 * ============================================================================ */

void setup()
{
    g_boot_count++;

#ifdef DEBUG
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);
    print_log("\n========================================\n");
    print_log("[NODE %d] Boot #%u\n", NODE_ID, g_boot_count);
    print_log("========================================\n");
#endif

    init_lora_radio();
    init_sensors();

    // Store initial readings on first boot
    if (g_boot_count == 1) {
        float temp;
        uint16_t lux;
        read_all_sensors(g_prev_humidity, g_prev_distance, temp, lux);
    }
}

/* ============================================================================
 * MAIN LOOP
 * ============================================================================ */

void loop()
{
    // Read sensors
    float humidity, distance, temperature;
    uint16_t luminosity;
    read_all_sensors(humidity, distance, temperature, luminosity);
    
    bool presence_detected = (distance < MAX_DISTANCE_TO_BE_PRESENCE_CM);

#ifdef DEBUG
    print_log("[SENSORS] Moisture=%.1f%%, Distance=%.0fcm, Temp=%.1f°C, Lux=%u, Presence=%s\n", 
          humidity, distance, temperature, luminosity, presence_detected ? "YES" : "No");
#endif

    // Decide whether to transmit
    bool should_send = true;

#if ADAPTIVE_TX_ENABLED
    should_send = should_transmit(humidity, distance);
    if (!should_send) {
#ifdef DEBUG
        print_log("[TX] Skipped - values unchanged\n");
#endif
        g_tx_stats.skipped++;
    }
#endif

    // Transmit if needed
    if (should_send) {
        if (transmit_sensor_data(humidity, distance, temperature, luminosity)) {
            g_tx_stats.success++;
            g_prev_humidity = humidity;
            g_prev_distance = distance;
        } else {
            g_tx_stats.failed++;
        }
    }

    g_tx_stats.total++;

#ifdef DEBUG
    print_statistics();
#endif

    // Sleep or delay until next cycle
#if DEEP_SLEEP_ENABLED
#ifdef DEBUG
    print_log("[POWER] Entering deep sleep for %d seconds\n", TX_INTERVAL_MS / 1000);
#endif
    delay(50);
    enter_deep_sleep();
#else
    delay(TX_INTERVAL_MS);
#endif
}

/* ============================================================================
 * LORA INITIALIZATION
 * ============================================================================ */

static void init_lora_radio()
{
#ifdef DEBUG
    print_log("[LORA] Initializing: %.1f MHz, SF%d, %d dBm\n", 
          LORA_FREQUENCY_MHZ, LORA_SPREADING_FACTOR, LORA_TX_POWER);
#endif

    // Hardware reset
    pinMode(LORA_PIN_RST, OUTPUT);
    digitalWrite(LORA_PIN_RST, LOW);
    delay(10);
    digitalWrite(LORA_PIN_RST, HIGH);
    delay(10);

    // Initialize SPI
    SPI.begin(LORA_PIN_SCK, LORA_PIN_MISO, LORA_PIN_MOSI, LORA_PIN_CS);
    SPI.setFrequency(2000000);
    delay(100);

    // Configure radio
    int result = g_radio.begin(
        LORA_FREQUENCY_MHZ,
        LORA_BANDWIDTH_KHZ,
        LORA_SPREADING_FACTOR,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        LORA_TX_POWER,
        LORA_PREAMBLE_LENGTH
    );

    if (result == RADIOLIB_ERR_NONE) {
        g_lora_ready = true;
        g_radio.setCurrentLimit(140);
#ifdef DEBUG
        print_log("[LORA] Initialized successfully\n");
#endif
    } else {
        g_lora_ready = false;
#ifdef DEBUG
        print_log("[LORA] FAILED to initialize (error %d)\n", result);
#endif
    }
}

/* ============================================================================
 * SENSOR INITIALIZATION
 * ============================================================================ */

static void init_sensors()
{
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
    if (g_lux_sensor.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
#ifdef DEBUG
        print_log("[SENSORS] BH1750 luminosity sensor initialized\n");
#endif
    } else {
#ifdef DEBUG
        print_log("[SENSORS] WARNING: BH1750 initialization failed!\n");
#endif
    }
    
#ifdef DEBUG
    print_log("[SENSORS] Hardware sensors initialized\n");
#endif
#else
#ifdef DEBUG
    print_log("[SENSORS] Running in SIMULATION mode\n");
#endif
#endif
}

/* ============================================================================
 * SOIL MOISTURE SENSOR (MH-RD)
 * ============================================================================ */

static float read_soil_moisture()
{
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

/* ============================================================================
 * ULTRASONIC DISTANCE SENSOR (HC-SR04)
 * ============================================================================ */

static float read_ultrasonic_distance()
{
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

/* ============================================================================
 * TEMPERATURE SENSOR (simulated for now, can be DHT or other)
 * ============================================================================ */

static float read_temperature()
{
#if REAL_SENSORS_ENABLED
    // TODO: Implement actual temperature sensor reading
    // For now, return a reasonable default or implement with DHT sensor
    return 25.0f;  // Default temperature
#else
    return generate_simulated_value(SIM_TEMPERATURE_BASE, SIM_TEMPERATURE_VARIATION);
#endif
}

/* ============================================================================
 * LUMINOSITY SENSOR (BH1750)
 * ============================================================================ */

static uint16_t read_luminosity()
{
#if REAL_SENSORS_ENABLED
    float lux = g_lux_sensor.readLightLevel();
    if (lux < 0) {
#ifdef DEBUG
        print_log("[SENSORS] BH1750 read error, returning 0\n");
#endif
        return 0;
    }
    return static_cast<uint16_t>(constrain(lux, 0.0f, 65535.0f));
#else
    float lux = generate_simulated_value(SIM_LUMINOSITY_BASE, SIM_LUMINOSITY_VARIATION);
    return static_cast<uint16_t>(constrain(lux, 0.0f, 65535.0f));
#endif
}

/* ============================================================================
 * READ ALL SENSORS
 * ============================================================================ */

static void read_all_sensors(float& out_humidity, float& out_distance, float& out_temperature, uint16_t& out_luminosity)
{
    out_humidity = read_soil_moisture();
    out_distance = read_ultrasonic_distance();
    out_temperature = read_temperature();
    out_luminosity = read_luminosity();
    
    // Ensure values are within valid ranges
    out_humidity = constrain(out_humidity, 0.0f, 100.0f);
    out_distance = constrain(out_distance, 5.0f, 400.0f);
    out_temperature = constrain(out_temperature, -40.0f, 80.0f);
}

/* ============================================================================
 * ADAPTIVE TRANSMISSION LOGIC
 * ============================================================================ */

static bool should_transmit(float humidity, float distance)
{
    // Always transmit on first boot
    if (g_boot_count == 1) {
        return true;
    }
    
    // Force transmission every 10th cycle (heartbeat)
    if (g_boot_count % 10 == 0) {
        return true;
    }
    
    // Transmit if humidity changed significantly
    if (fabsf(humidity - g_prev_humidity) > HUMIDITY_CHANGE_THRESHOLD) {
        return true;
    }
    
    // Transmit if distance changed significantly
    if (fabsf(distance - g_prev_distance) > DISTANCE_CHANGE_THRESHOLD) {
        return true;
    }
    
    return false;
}

/* ============================================================================
 * LORA TRANSMISSION
 * ============================================================================ */

static bool transmit_sensor_data(float humidity, float distance, float temperature, uint16_t luminosity)
{
    if (!g_lora_ready) {
#ifdef DEBUG
        print_log("[TX] ERROR: LoRa radio not initialized\n");
#endif
        return false;
    }

    // Build message
    SensorDataMessage msg;
    msg.msg_type      = MSG_TYPE_SENSOR_DATA;
    msg.client_id     = NODE_ID;
    msg.timestamp     = millis();
    msg.temperature   = encode_temperature(temperature);
    msg.humidity      = encode_humidity(humidity);
    msg.distance_cm   = static_cast<uint16_t>(distance);
    msg.battery       = 100;  // TODO: implement battery monitoring
    msg.luminosity_lux = luminosity;
    msg.reserved      = 0;
    msg.checksum      = calculate_checksum(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));

    // Transmit with retries
    for (int attempt = 1; attempt <= TX_MAX_RETRIES; attempt++) {
        int result = g_radio.transmit(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));
        
        if (result == RADIOLIB_ERR_NONE) {
#ifdef DEBUG
            print_log("[TX] Transmission successful\n");
#endif
            return true;
        }
#ifdef DEBUG
        print_log("[TX] Attempt %d failed (error %d)\n", attempt, result);
#endif
        delay(100);
    }
#ifdef DEBUG
    print_log("[TX] All retry attempts failed\n");
#endif
    return false;
}

/* ============================================================================
 * POWER MANAGEMENT
 * ============================================================================ */

static void enter_deep_sleep()
{
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME_US);
    esp_deep_sleep_start();
}

/* ============================================================================
 * STATISTICS & DEBUGGING
 * ============================================================================ */

static void print_statistics()
{
#ifdef DEBUG
    if (g_tx_stats.total > 0) {
        float success_rate = (static_cast<float>(g_tx_stats.success) / g_tx_stats.total) * 100.0f;
        print_log("[STATS] Total=%u, Success=%u, Failed=%u, Skipped=%u (%.0f%% success)\n",
              g_tx_stats.total, g_tx_stats.success, g_tx_stats.failed, 
              g_tx_stats.skipped, success_rate);
    }
#endif
}

/* ============================================================================
 * SIMULATION UTILITIES
 * ============================================================================ */

static float generate_simulated_value(float base_value, float variation)
{
    randomSeed(analogRead(0) + millis());
    float random_factor = static_cast<float>(random(-1000, 1000)) / 1000.0f;
    return base_value + (random_factor * variation);
}
