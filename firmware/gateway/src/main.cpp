#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>

#include "constants.h"
#include "message_struct.h"
#include "lora.h"
#include "wifi.h"
#include "batch.h"
#include "processing.h"
#include "energy_manager.h"
#include "utils.h"


static uint32_t last_stats_time = 0;

#ifdef SIMUL_DATA
static uint32_t last_simul_time = 0;
static void generate_fake_packet();
#endif

static void print_statistics();

void setup() {
    print_log("Node %d - Setting up...\n", NODE_ID);

    energy.start_time = millis();
    energy.last_calc_time = millis();

    LoRaRadio::get_instance().setup();

    init_wifi();

    print_log("Initialized\n");
    last_stats_time = millis();
}

void loop() {
#ifdef SIMUL_DATA
    if (millis() - last_simul_time >= SIMUL_PERIOD_MS) {
        generate_fake_packet();
        last_simul_time = millis();
    }
#else
    LoRaRadio::get_instance().check_packets();
#endif

    update_energy_consumption();

#ifdef BATCH_ON
    if (batch_count > 0 && (millis() - batch_start_time >= BATCH_TIMEOUT_MS)) {
        print_log("Flushing batch, timeout reached...");
        flush_batch();
    }
#endif

    if (millis() - last_stats_time >= STATS_PERIOD_MS) {
        print_statistics();
        send_gateway_statistics();
        last_stats_time = millis();
    }

    check_wifi_connection();
}

static void print_statistics() {
    LoRaRadio::Stats lora_stats = LoRaRadio::get_instance().get_stats();
    
    uint32_t uptime_s = (millis() - energy.start_time) / 1000;
    float avg_latency = latency.samples > 0
        ? (static_cast<float>(latency.total_ms) / latency.samples) : 0.0f;
    float packet_loss = lora_stats.total_rx_packets > 0
        ? (static_cast<float>(lora_stats.total_rx_invalids) / lora_stats.total_rx_packets * 100.0f) : 0.0f;
    float server_success = server_stats.total > 0
        ? (static_cast<float>(server_stats.success) / server_stats.total * 100.0f) : 0.0f;

    print_log("\n\nStatistics of gateway:\n");
    print_log("Uptime: %02u:%02u:%02u\n", uptime_s / 3600, (uptime_s % 3600) / 60, uptime_s % 60);
    print_log("LoRa RX - Valid: %u | Invalid: %u | Duplicates: %u | Loss: %.1f%%\n",
          lora_stats.total_rx_valids, lora_stats.total_rx_invalids, get_duplicate_count(), packet_loss);
    print_log("Server TX - Success: %u/%u | Rate: %.1f%%\n",
          server_stats.success, server_stats.total, server_success);
    if (latency.samples > 0) {
        print_log("Latency - Avg: %.0f ms | Range: %u-%u ms\n", avg_latency,
              latency.min_ms == UINT32_MAX ? 0 : latency.min_ms, latency.max_ms);
    }
    print_log("Energy consumption: %.2f mAh\n", energy.total_mah);
#ifdef WIFI_ON
    if (wifi_connected) {
        print_log("WiFi signal strength: %d dBm\n", get_current_wifi_rssi());
    }
#endif
    print_log("\n\n");
}

#ifdef SIMUL_DATA
static void generate_fake_packet() {
    print_log("\nGenerating simulated sensor packet\n");

    SensorDataMessage test_msg;
    test_msg.msg_type = MSG_TYPE_SENSOR_DATA;
    test_msg.client_id = 42;
    test_msg.timestamp = millis() / 1000;
    test_msg.humidity = random(3000, 8000);
    test_msg.distance_cm = random(5, 200);
    test_msg.temperature = 0;
    test_msg.battery = random(60, 100);
    test_msg.reserved = 0;

    uint8_t* data = reinterpret_cast<uint8_t*>(&test_msg);
    test_msg.checksum = calculate_checksum(data, sizeof(SensorDataMessage));

    float fake_rssi = static_cast<float>(random(-90, -30));
    float fake_snr = static_cast<float>(random(5, 12));

    LoRaRadio::Stats lora_stats = LoRaRadio::get_instance().get_stats();

    lora_stats.total_rx_packets++;

    print_log("Fake data - Moisture: %.1f%% | Distance: %dcm | Presence: %s\n",
          test_msg.humidity / 100.0f,
          test_msg.distance_cm,
          (test_msg.distance_cm < 100) ? "YES" : "No");

    process_rx_lora_message(
        reinterpret_cast<uint8_t*>(&test_msg),
        sizeof(SensorDataMessage),
        fake_rssi,
        fake_snr
    );

    send_gateway_statistics();
}
#endif
