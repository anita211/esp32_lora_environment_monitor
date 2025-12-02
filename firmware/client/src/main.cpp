#include <Arduino.h>
#include <RadioLib.h>
#include <math.h>
#include "constants.h"
#include "message_struct.h"
#include "utils.h"
#include "lora.h"
#include "sensors.h"


// Persisted across deep sleep (stored in RTC memory)
RTC_DATA_ATTR static uint32_t boot_count = 0;


static bool should_transmit(float humidity, float distance);
static bool transmit_sensor_data(float humidity, float distance, float temperature, uint16_t luminosity);

static void enter_deep_sleep();

static void print_statistics();

void setup()
{
    boot_count++;
    delay(10000);

    print_log("\nNode %d initializing, boot #%u\n", NODE_ID, boot_count);

    LoRaRadio::get_instance().setup();
    Sensors::get_instance().setup();

    // Store initial readings on first boot
    if (boot_count == 1) {
        float temp;
        uint16_t lux;
        float humidity, distance;
        Sensors::get_instance().read_all(humidity, distance, temp, lux);
        Sensors::get_instance().set_prev_humidity(humidity);
        Sensors::get_instance().set_prev_distance(distance);
    }
}

void loop()
{
    float humidity, distance, temperature;
    uint16_t luminosity;
    Sensors::get_instance().read_all(humidity, distance, temperature, luminosity);
    
    bool presence_detected = (distance < MAX_DISTANCE_TO_BE_PRESENCE_CM);

    print_log("Moisture %.1f%%, distance %.0fcm, temperature %.1f°C, luminosity %u lux, presence %s\n", 
          humidity, distance, temperature, luminosity, presence_detected ? "detected" : "not detected");
    
    bool should_send = true;

#if ADAPTIVE_TX_ENABLED
    should_send = should_transmit(humidity, distance);
    if (!should_send) {
        print_log("Skipping transmission, values unchanged\n");
        LoRaRadio::get_instance().increment_skipped();
    }
#endif

    if (should_send) {
        if (transmit_sensor_data(humidity, distance, temperature, luminosity)) {
            Sensors::get_instance().set_prev_humidity(humidity);
            Sensors::get_instance().set_prev_distance(distance);
        }
    }

    LoRaRadio::get_instance().increment_total();

    print_statistics();

#if DEEP_SLEEP_ENABLED
    print_log("Deep sleep mode starting for %d seconds\n", TX_INTERVAL_MS / 1000);
    delay(50);
    enter_deep_sleep();
#else
    delay(TX_INTERVAL_MS);
#endif
}

static bool should_transmit(float humidity, float distance)
{
    if (boot_count == 1) {
        return true;
    }
    
    if (boot_count % 10 == 0) {
        return true;
    }
    
    if (fabsf(humidity - Sensors::get_instance().get_prev_humidity()) > HUMIDITY_CHANGE_THRESHOLD) {
        return true;
    }
    
    if (fabsf(distance - Sensors::get_instance().get_prev_distance()) > DISTANCE_CHANGE_THRESHOLD) {
        return true;
    }
    
    return false;
}

static bool transmit_sensor_data(float humidity, float distance, float temperature, uint16_t luminosity)
{
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

    return LoRaRadio::get_instance().transmit(reinterpret_cast<uint8_t*>(&msg), sizeof(msg));
}

static void enter_deep_sleep()
{
    esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME_US);
    esp_deep_sleep_start();
}

static void print_statistics()
{
    LoRaRadio::Stats lora_stats = LoRaRadio::get_instance().get_stats();
    
    if (lora_stats.total_tx_packets > 0) {
        float success_rate = (static_cast<float>(lora_stats.total_tx_success) / lora_stats.total_tx_packets) * 100.0f;
        print_log("Transmission stats: %u total, %u successful, %u failed, %u skipped (%.0f%% success rate)\n",
              lora_stats.total_tx_packets, lora_stats.total_tx_success, lora_stats.total_tx_failed, 
              lora_stats.total_tx_skipped, success_rate);
    }
}
