#include "wifi.h"
#include "constants.h"
#include "utils.h"
#include "processing.h"
#include "message_struct.h"
#include "lora.h"
#include "energy_manager.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>


bool wifi_connected = false;
bool time_synced = false;

struct server_stats_t server_stats = {0, 0, 0};

struct latency_t latency = {0, 0, UINT32_MAX, 0, 0};


void init_wifi() {
#ifdef WIFI_ON
    print_log("Connecting to WiFi SSID: %s\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_TIMEOUT_MS) {
        delay(500);
        print_log(".");
    }
    print_log("\n");

    if (WiFi.status() == WL_CONNECTED) {
        wifi_connected = true;
        print_log("WiFi connected - IP: %s\n", WiFi.localIP().toString().c_str());

        configTime(-3 * 3600, 0, "pool.ntp.org");  // UTC-3 (Brazil)
        int retry = 0;
        while (time(nullptr) < 100000 && retry < 10) {
            delay(500);
            retry++;
        }
        time_synced = (time(nullptr) > 100000);
        print_log("Time sync: %s\n", time_synced ? "OK" : "FAILED");
    } else {
        wifi_connected = false;
        print_log("WiFi connection FAILED (status: %d)\n", WiFi.status());
    }
    
    print_log("wifi_connected = %s\n", wifi_connected ? "true" : "false");
#else
    print_log("WiFi is disabled (WIFI_ON not defined)\n");
#endif
}

void forward_to_server(const char* json_data) {
#ifdef WIFI_ON
    if (!wifi_connected) {
        print_log("forward_to_server: WiFi not connected, skipping\n");
        return;
    }

    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + SERVER_ENDPOINT_DATA;
    
    print_log("Sending to: %s\n", url.c_str());

    uint32_t start_time = millis();

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);  // Timeout de 5 segundos
    int http_code = http.POST(json_data);

    latency.last_ms = millis() - start_time;
    server_stats.total++;

    if (http_code > 0) {
        latency.total_ms += latency.last_ms;
        latency.samples++;
        if (latency.last_ms < latency.min_ms) latency.min_ms = latency.last_ms;
        if (latency.last_ms > latency.max_ms) latency.max_ms = latency.last_ms;

        if (http_code == HTTP_CODE_OK || http_code == HTTP_CODE_CREATED) {
            server_stats.success++;
            print_log("Forward to server success: %u ms (code: %d)\n", latency.last_ms, http_code);
        } else {
            server_stats.failed++;
            print_log("Server response error code: %d\n", http_code);
        }
    } else {
        server_stats.failed++;
        print_log("HTTP request failed: %s (code: %d)\n", http.errorToString(http_code).c_str(), http_code);
    }

    http.end();
#else
    print_log("forward_to_server: WIFI_ON not defined\n");
#endif
}

void send_gateway_statistics() {
#ifdef WIFI_ON
    if (!wifi_connected) return;

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
    if (time_synced) {
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

    uint32_t uptime_seconds = (millis() - energy.start_time) / 1000;
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
    server["tx_total"] = server_stats.total;
    server["tx_success"] = server_stats.success;
    server["tx_failed"] = server_stats.failed;
    server["success_rate_percent"] = server_stats.total > 0
        ? (static_cast<float>(server_stats.success) / server_stats.total * 100.0f) : 0.0f;

    // latency_json statistics
    JsonObject latency_json = doc["latency_json"].to<JsonObject>();
    latency_json["avms"] = latency.samples > 0
        ? (static_cast<float>(latency.total_ms) / latency.samples) : 0.0f;
    latency_json["min_ms"] = latency.min_ms == UINT32_MAX ? 0 : latency.min_ms;
    latency_json["max_ms"] = latency.max_ms;
    latency_json["last_ms"] = latency.last_ms;
    latency_json["samples"] = latency.samples;

    // Energy consumption
    doc["energy_mah"] = energy.total_mah;

    // WiFi signal
    if (wifi_connected) {
        doc["wifi_rssi"] = WiFi.RSSI();
    }

    String json;
    serializeJson(doc, json);
    return json;
}

void check_wifi_connection() {
  #ifdef WIFI_ON
  if (!WiFi.isConnected() && wifi_connected) {
      print_log("WiFi disconnected, trying to connect again...");
      wifi_connected = false;
      init_wifi();
  }
  #endif
}

int get_current_wifi_rssi() {
#ifdef WIFI_ON
    if (wifi_connected) {
        return WiFi.RSSI();
    } else {
        return 0;
    }
#else
    return 0;
#endif
}
