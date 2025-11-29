#include "config.h"
#include "protocol.h"
#include <Arduino.h>
#include <RadioLib.h>
#include <math.h>

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

float prev_humidity = 0.0f;
float prev_distance = 0.0f;

uint32_t tx_count = 0;
uint32_t tx_success = 0;
uint32_t tx_failed = 0;
uint32_t tx_skipped = 0;

bool lora_initialized = false;

RTC_DATA_ATTR uint32_t boot_count = 0;

void setup_lora();
void setup_sensors();
float read_moisture_sensor();
float read_ultrasonic_distance();
void read_sensors(float& humid, float& distance);
bool should_transmit(float humid, float distance);
bool transmit_sensor_data(float humid, float distance);
void enter_deep_sleep();
void print_stats();
float simulate_sensor_reading(float base, float variation);

void setup()
{
    boot_count++;

#if DEBUG_MODE
    Serial.begin(SERIAL_BAUD);
    delay(500);
    DEBUG_PRINTF("\n[CLIENT] Boot #%u, ID: %d\n", boot_count, CLIENT_ID);
#endif

    setup_lora();
    setup_sensors();

    if (boot_count == 1) {
        read_sensors(prev_humidity, prev_distance);
    }
}

void loop()
{
    float humidity, distance;
    read_sensors(humidity, distance);
    bool presence = distance < PRESENCE_THRESHOLD_CM;

    DEBUG_PRINTF("[SENSOR] H=%.1f%%, D=%.0fcm, P=%s\n", 
                humidity, distance, presence ? "YES" : "No");

    bool should_send = true;

#if ENABLE_ADAPTIVE_TX
    should_send = should_transmit(humidity, distance);
    if (!should_send) {
        DEBUG_PRINTLN("[TX] Skipped (no change)");
        tx_skipped++;
    }
#endif

    if (should_send) {
        if (transmit_sensor_data(humidity, distance)) {
            tx_success++;
            prev_humidity = humidity;
            prev_distance = distance;
        } else {
            tx_failed++;
        }
    }

    tx_count++;

#if DEBUG_MODE
    print_stats();
#endif

#if ENABLE_DEEP_SLEEP
    DEBUG_PRINTF("[SLEEP] %ds\n", TX_INTERVAL_MS / 1000);
    delay(50);
    enter_deep_sleep();
#else
    delay(TX_INTERVAL_MS);
#endif
}

void setup_lora()
{
    DEBUG_PRINTF("[LORA] Init %.1fMHz, SF%d, %ddBm\n", 
                LORA_FREQUENCY, LORA_SPREADING, LORA_TX_POWER);

    pinMode(LORA_RST, OUTPUT);
    digitalWrite(LORA_RST, LOW);
    delay(10);
    digitalWrite(LORA_RST, HIGH);
    delay(10);

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
    SPI.setFrequency(2000000);
    delay(100);

    int state = radio.begin(
        LORA_FREQUENCY,
        LORA_BANDWIDTH,
        LORA_SPREADING,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        LORA_TX_POWER,
        LORA_PREAMBLE);

    if (state == RADIOLIB_ERR_NONE) {
        lora_initialized = true;
        radio.setCurrentLimit(140);
        DEBUG_PRINTLN("[LORA] OK");
    } else {
        lora_initialized = false;
        DEBUG_PRINTF("[LORA] FAILED (%d)\n", state);
    }
}

void setup_sensors()
{
#if USE_REAL_SENSORS
    pinMode(ULTRASONIC_TRIG_PIN, OUTPUT);
    pinMode(ULTRASONIC_ECHO_PIN, INPUT);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    pinMode(MOISTURE_SENSOR_PIN, INPUT);
    analogReadResolution(12);
    DEBUG_PRINTLN("[SENSOR] Real sensors ready");
#else
    DEBUG_PRINTLN("[SENSOR] Simulation mode");
#endif
}

float read_moisture_sensor()
{
#if USE_REAL_SENSORS
    uint32_t sum = 0;
    for (int i = 0; i < MOISTURE_SAMPLES; i++) {
        sum += analogRead(MOISTURE_SENSOR_PIN);
        delay(10);
    }
    uint16_t avg = sum / MOISTURE_SAMPLES;
    float range = (float)(MOISTURE_DRY_VALUE - MOISTURE_WET_VALUE);
    float humidity = 100.0f - ((avg - MOISTURE_WET_VALUE) / range * 100.0f);
    return constrain(humidity, 0.0f, 100.0f);
#else
    return simulate_sensor_reading(HUMID_BASE, HUMID_VARIATION);
#endif
}

float read_ultrasonic_distance()
{
#if USE_REAL_SENSORS
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(ULTRASONIC_TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(ULTRASONIC_TRIG_PIN, LOW);
    long duration = pulseIn(ULTRASONIC_ECHO_PIN, HIGH, ULTRASONIC_TIMEOUT_US);
    float distance = (duration * 0.0343f) / 2.0f;
    return constrain(distance, 2.0f, 400.0f);
#else
    return simulate_sensor_reading(DISTANCE_BASE, DISTANCE_VARIATION);
#endif
}

void read_sensors(float& humid, float& distance)
{
    humid = read_moisture_sensor();
    distance = read_ultrasonic_distance();
    humid = constrain(humid, 0.0f, 100.0f);
    distance = constrain(distance, 5.0f, 400.0f);
}

bool should_transmit(float humid, float distance)
{
    if (boot_count == 1) return true;
    if (boot_count % 10 == 0) return true;
    if (fabsf(humid - prev_humidity) > HUMID_THRESHOLD) return true;
    if (fabsf(distance - prev_distance) > DISTANCE_THRESHOLD) return true;
    return false;
}

bool transmit_sensor_data(float humid, float distance)
{
    if (!lora_initialized) {
        DEBUG_PRINTLN("[TX] LoRa not ready");
        return false;
    }

    SensorDataMessage msg;
    msg.msg_type = MSG_TYPE_SENSOR_DATA;
    msg.client_id = CLIENT_ID;
    msg.timestamp = millis();
    msg.temperature = 0;
    msg.humidity = encode_humidity(humid);
    msg.distance_cm = static_cast<uint16_t>(distance);
    msg.battery = 100;
    msg.reserved = 0;
    msg.checksum = calculate_checksum(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));

    for (int attempt = 0; attempt < MAX_TX_RETRIES; attempt++) {
        int state = radio.transmit(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));
        if (state == RADIOLIB_ERR_NONE) {
            DEBUG_PRINTLN("[TX] OK");
            return true;
        }
        DEBUG_PRINTF("[TX] Retry %d\n", attempt + 1);
        delay(100);
    }

    DEBUG_PRINTLN("[TX] FAILED");
    return false;
}

void enter_deep_sleep()
{
    esp_sleep_enable_timer_wakeup(SLEEP_TIME_US);
    esp_deep_sleep_start();
}

void print_stats()
{
    if (tx_count > 0) {
        float success_rate = (float)tx_success / tx_count * 100;
        DEBUG_PRINTF("[STATS] TX:%u OK:%u FAIL:%u SKIP:%u (%.0f%%)\n",
                     tx_count, tx_success, tx_failed, tx_skipped, success_rate);
    }
}

float simulate_sensor_reading(float base, float variation)
{
    randomSeed(analogRead(0) + millis());
    float factor = (float)random(-1000, 1000) / 1000.0f;
    return base + (factor * variation);
}
