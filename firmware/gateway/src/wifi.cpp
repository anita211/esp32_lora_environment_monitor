#include "wifi.h"
#include "constants.h"
#include "utils.h"
#include "processing.h"
#include "protocol.h"
#include "lora.h"
#include "energy_manager.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>


bool g_wifi_connected = false;
bool g_time_synced = false;

struct g_server_stats_t g_server_stats = {0, 0, 0};

struct g_latency_t g_latency = {0, 0, UINT32_MAX, 0, 0};


void init_wifi() {
#ifdef WIFI_ON
    print_log("Connecting to WiFi SSID: %s\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_TIMEOUT_MS) {
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        g_wifi_connected = true;
        print_log("Connected - IP: %s\n", WiFi.localIP().toString().c_str());

        configTime(-3 * 3600, 0, "pool.ntp.org");  // UTC-3 (Brazil)
        int retry = 0;
        while (time(nullptr) < 100000 && retry < 10) {
            delay(500);
            retry++;
        }
        g_time_synced = (time(nullptr) > 100000);
        print_log("Synchronization of time: %s\n", g_time_synced ? "OK" : "FAILED");
    } else {
        g_wifi_connected = false;
        print_log("Couldn't connect to WiFi\n");
    }
#endif
}

void forward_to_server(const char* json_data) {
#ifdef WIFI_ON
    if (!g_wifi_connected) return;

    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + SERVER_ENDPOINT_DATA;

    uint32_t start_time = millis();

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int http_code = http.POST(json_data);

    g_latency.last_ms = millis() - start_time;
    g_server_stats.total++;

    if (http_code > 0) {
        g_latency.total_ms += g_latency.last_ms;
        g_latency.samples++;
        if (g_latency.last_ms < g_latency.min_ms) g_latency.min_ms = g_latency.last_ms;
        if (g_latency.last_ms > g_latency.max_ms) g_latency.max_ms = g_latency.last_ms;

        if (http_code == HTTP_CODE_OK || http_code == HTTP_CODE_CREATED) {
            g_server_stats.success++;
            print_log("Foward to server success: %u ms\n", g_latency.last_ms);
        } else {
            g_server_stats.failed++;
            print_log("Error response code: %d\n", http_code);
        }
    } else {
        g_server_stats.failed++;
        print_log("Forwarding to server failed: %s\n", http.errorToString(http_code).c_str());
    }

    http.end();
#endif
}

void send_gateway_statistics() {
#ifdef WIFI_ON
    if (!g_wifi_connected) return;

    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + SERVER_ENDPOINT_STATS;
    String stats_json = build_gateway_stats_json();

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int http_code = http.POST(stats_json);

    print_log("[STATS] Sent to server - Response code: %d\n", http_code);
    http.end();
#endif
}

String get_iso8601_timestamp() {
    if (g_time_synced) {
        struct timeval tv;
        gettimeofday(&tv, NULL);

        struct tm timeinfo;
        localtime_r(&tv.tv_sec, &timeinfo);

        char buffer[30];
        strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);

        char timestamp[40];
        snprintf(timestamp, sizeof(timestamp), "%s.%03ldZ", buffer, tv.tv_usec / 1000);

        return String(timestamp);
    } else {
        char buffer[30];
        snprintf(buffer, sizeof(buffer), "boot+%lu", millis());
        return String(buffer);
    }
}

String build_gateway_stats_json() {
    StaticJsonDocument<512> doc;
    LoRaRadio::Stats lora_stats = LoRaRadio::get_instance().get_stats();

    doc["type"] = "gateway_stats";
    doc["NODE_ID"] = NODE_ID;
    doc["timestamp"] = get_iso8601_timestamp();

    uint32_t uptime_seconds = (millis() - g_energy.start_time) / 1000;
    doc["uptime_seconds"] = uptime_seconds;

    // LoRa statistics
    JsonObject lora = doc["lora_stats"].to<JsonObject>();
    lora["rx_total"] = lora_stats.total_rx_packets;
    lora["rx_valid"] = lora_stats.total_rx_valids;
    lora["rx_invalid"] = lora_stats.total_rx_invalids;
    lora["rx_checksum_error"] = lora_stats.total_checksum_errors;
    lora["packet_loss_percent"] = lora_stats.total_rx_packets > 0
        ? (static_cast<float>(lora_stats.total_rx_invalids) / lora_stats.total_rx_packets * 100.0f) : 0.0f;

    // Server statistics
    JsonObject server = doc["server_stats"].to<JsonObject>();
    server["tx_total"] = g_server_stats.total;
    server["tx_success"] = g_server_stats.success;
    server["tx_failed"] = g_server_stats.failed;
    server["success_rate_percent"] = g_server_stats.total > 0
        ? (static_cast<float>(g_server_stats.success) / g_server_stats.total * 100.0f) : 0.0f;

    // Latency statistics
    JsonObject latency = doc["latency"].to<JsonObject>();
    latency["avg_ms"] = g_latency.samples > 0
        ? (static_cast<float>(g_latency.total_ms) / g_latency.samples) : 0.0f;
    latency["min_ms"] = g_latency.min_ms == UINT32_MAX ? 0 : g_latency.min_ms;
    latency["max_ms"] = g_latency.max_ms;
    latency["last_ms"] = g_latency.last_ms;
    latency["samples"] = g_latency.samples;

    // Energy consumption
    doc["energy_mah"] = g_energy.total_mah;

    // WiFi signal
    if (g_wifi_connected) {
        doc["wifi_rssi"] = WiFi.RSSI();
    }

    String json;
    serializeJson(doc, json);
    return json;
}
