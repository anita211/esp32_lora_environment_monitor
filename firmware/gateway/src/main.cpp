#include <Arduino.h>
#include <ArduinoJson.h>
#include <RadioLib.h>
#include <time.h>
#include <sys/time.h>

#include "config.h"
#include "protocol.h"

#if ENABLE_WIFI
#include <HTTPClient.h>
#include <WiFi.h>
#endif

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);

uint32_t rx_count = 0;
uint32_t rx_valid = 0;
uint32_t rx_invalid = 0;
uint32_t rx_checksum_error = 0;

uint32_t tx_to_server_count = 0;
uint32_t tx_to_server_success = 0;
uint32_t tx_to_server_failed = 0;

uint32_t total_latency_ms = 0;
uint32_t latency_samples = 0;
uint32_t min_latency_ms = UINT32_MAX;
uint32_t max_latency_ms = 0;
uint32_t last_latency_ms = 0;

uint32_t uptime_start = 0;
float total_energy_mah = 0.0;
uint32_t last_energy_calc_time = 0;

const float ACTIVE_CURRENT_MA = 120.0;
const float WIFI_TX_CURRENT_MA = 220.0;
const float IDLE_CURRENT_MA = 20.0;

uint32_t last_stats_time = 0;
uint32_t last_rx_time = 0;

#if TEST_MODE
uint32_t last_test_time = 0;
#endif

bool wifi_connected = false;

bool time_synced = false;

uint8_t rx_buffer[MAX_PACKET_SIZE];

#if ENABLE_BATCH_MODE
String batch_buffer[BATCH_SIZE];
uint8_t batch_count = 0;
uint32_t batch_start_time = 0;
#endif

String get_iso8601_timestamp();
void update_energy_consumption();
String get_gateway_stats_json();

void setup_lora();
void setup_wifi();
void listen_for_messages();
void process_message(uint8_t* data, size_t length, float rssi, float snr);
void process_sensor_data(SensorDataMessage* msg, float rssi, float snr);
void forward_to_server(const char* json_data);
void send_gateway_stats();
void print_statistics();
String message_to_json(SensorDataMessage* msg, float rssi, float snr);
#if ENABLE_BATCH_MODE
void add_to_batch(const String& json);
void flush_batch();
#endif

#if TEST_MODE
void send_test_packet();
#endif

void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(500);

    DEBUG_PRINTF("\n[GATEWAY] ID: %d\n", GATEWAY_ID);

    uptime_start = millis();
    last_energy_calc_time = millis();

    setup_lora();
#if ENABLE_WIFI
    setup_wifi();
#endif

    DEBUG_PRINTLN("[GATEWAY] Ready\n");
    last_stats_time = millis();
}

void loop()
{
#if TEST_MODE
    if (millis() - last_test_time >= TEST_INTERVAL_MS) {
        send_test_packet();
        last_test_time = millis();
    }
#else
    listen_for_messages();
#endif

    update_energy_consumption();

#if ENABLE_BATCH_MODE
    if (batch_count > 0 && (millis() - batch_start_time >= BATCH_TIMEOUT_MS)) {
        DEBUG_PRINTLN("[BATCH] Timeout, flushing...");
        flush_batch();
    }
#endif

    if (millis() - last_stats_time >= STATS_INTERVAL_MS) {
        print_statistics();
        send_gateway_stats();
        last_stats_time = millis();
    }

#if ENABLE_WIFI
    if (!WiFi.isConnected() && wifi_connected) {
        DEBUG_PRINTLN("WiFi disconnected! Attempting to reconnect...");
        wifi_connected = false;
        setup_wifi();
    }
#endif
}

void setup_lora()
{
    DEBUG_PRINTF("[LORA] Init %.1fMHz, BW%.0f, SF%d\n", 
                 LORA_FREQUENCY, LORA_BANDWIDTH, LORA_SPREADING);

    int state = radio.begin(
        LORA_FREQUENCY,
        LORA_BANDWIDTH,
        LORA_SPREADING,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        10,
        LORA_PREAMBLE);

    if (state == RADIOLIB_ERR_NONE) {
        DEBUG_PRINTLN("[LORA] OK");
        radio.startReceive();
    } else {
        DEBUG_PRINTF("[LORA] FAILED (%d)\n", state);
        while (true) delay(1000);
    }
}

void setup_wifi()
{
#if ENABLE_WIFI
    DEBUG_PRINTF("[WIFI] Connecting to %s...\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_TIMEOUT_MS) {
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifi_connected = true;
        DEBUG_PRINTF("[WIFI] OK - %s\n", WiFi.localIP().toString().c_str());
        
        configTime(-3 * 3600, 0, "pool.ntp.org");
        int retry = 0;
        while (time(nullptr) < 100000 && retry < 10) {
            delay(500);
            retry++;
        }
        time_synced = (time(nullptr) > 100000);
        DEBUG_PRINTF("[NTP] %s\n", time_synced ? "OK" : "FAILED");
    } else {
        wifi_connected = false;
        DEBUG_PRINTLN("[WIFI] FAILED");
    }
#endif
}

void listen_for_messages()
{
    if (radio.getPacketLength() > 0) {
        int state = radio.readData(rx_buffer, MAX_PACKET_SIZE);

        if (state == RADIOLIB_ERR_NONE) {
            size_t length = radio.getPacketLength();
            float rssi = radio.getRSSI();
            float snr = radio.getSNR();

            rx_count++;
            last_rx_time = millis();

            DEBUG_PRINTF("[RX] #%u, %d bytes, RSSI=%.0f, SNR=%.1f\n", 
                         rx_count, length, rssi, snr);

            process_message(rx_buffer, length, rssi, snr);
        } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            DEBUG_PRINTLN("[RX] CRC error");
            rx_invalid++;
        }

        radio.startReceive();
    }
}

void process_message(uint8_t* data, size_t length, float rssi, float snr)
{
    if (length < 1) {
        rx_invalid++;
        return;
    }

    uint8_t msg_type = data[0];

    switch (msg_type) {
    case MSG_TYPE_SENSOR_DATA:
        if (length == sizeof(SensorDataMessage)) {
            if (verify_checksum(data, length)) {
                process_sensor_data((SensorDataMessage*)data, rssi, snr);
                rx_valid++;
            } else {
                DEBUG_PRINTLN("[RX] Checksum fail");
                rx_checksum_error++;
                rx_invalid++;
            }
        } else {
            DEBUG_PRINTF("[RX] Bad length: %d\n", length);
            rx_invalid++;
        }
        break;

    case MSG_TYPE_HEARTBEAT:
        if (length == sizeof(HeartbeatMessage) && verify_checksum(data, length)) {
            DEBUG_PRINTF("[RX] Heartbeat from %d\n", ((HeartbeatMessage*)data)->client_id);
            rx_valid++;
        } else {
            rx_invalid++;
        }
        break;

    case MSG_TYPE_ALERT:
        if (length == sizeof(AlertMessage) && verify_checksum(data, length)) {
            DEBUG_PRINTF("[RX] Alert from %d\n", ((AlertMessage*)data)->client_id);
            rx_valid++;
        } else {
            rx_invalid++;
        }
        break;

    default:
        DEBUG_PRINTF("[RX] Unknown type: 0x%02X\n", msg_type);
        rx_invalid++;
        break;
    }
}

void process_sensor_data(SensorDataMessage* msg, float rssi, float snr)
{
    const float humidity = decode_humidity(msg->humidity);
    const uint16_t distance = msg->distance_cm;
    const bool presence = distance < 100;

    DEBUG_PRINTF("[DATA] Node-%d: Humidity=%.1f%%, Distance=%dcm, Presence=%s, Battery=%d%%\n",
                 msg->client_id, humidity, distance, presence ? "YES" : "No", msg->battery);

    String json = message_to_json(msg, rssi, snr);

#if USE_HTTP && ENABLE_WIFI
    if (wifi_connected) {
#if ENABLE_BATCH_MODE
        add_to_batch(json);
#else
        forward_to_server(json.c_str());
#endif
    }
#endif
}

String get_iso8601_timestamp()
{
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

String message_to_json(SensorDataMessage* msg, float rssi, float snr)
{
    StaticJsonDocument<384> doc;

    char node_id[20];
    snprintf(node_id, sizeof(node_id), "node-%d", msg->client_id);
    doc["node_id"] = node_id;
    
    doc["gateway_id"] = GATEWAY_ID;
    
    doc["timestamp"] = get_iso8601_timestamp();
    doc["client_timestamp"] = msg->timestamp;

    const float humidity = decode_humidity(msg->humidity);
    const uint16_t distance = msg->distance_cm;
    const bool presence = distance < 100;

    JsonObject sensors = doc["sensors"].to<JsonObject>();
    sensors["humidity_percent"] = humidity;
    sensors["distance_cm"] = distance;
    sensors["presence_detected"] = presence;

    doc["battery_percent"] = msg->battery;

    JsonObject radio = doc["radio"].to<JsonObject>();
    radio["rssi_dbm"] = rssi;
    radio["snr_db"] = snr;

    String json;
    serializeJson(doc, json);

    return json;
}

void forward_to_server(const char* json_data)
{
#if USE_HTTP && ENABLE_WIFI
    if (!wifi_connected) return;

    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + SERVER_ENDPOINT;

    uint32_t start_time = millis();
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(json_data);
    
    last_latency_ms = millis() - start_time;
    tx_to_server_count++;

    if (httpCode > 0) {
        total_latency_ms += last_latency_ms;
        latency_samples++;
        if (last_latency_ms < min_latency_ms) min_latency_ms = last_latency_ms;
        if (last_latency_ms > max_latency_ms) max_latency_ms = last_latency_ms;
        
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
            tx_to_server_success++;
            DEBUG_PRINTF("[HTTP] OK (%ums)\n", last_latency_ms);
        } else {
            tx_to_server_failed++;
            DEBUG_PRINTF("[HTTP] Error %d\n", httpCode);
        }
    } else {
        tx_to_server_failed++;
        DEBUG_PRINTF("[HTTP] Failed: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
#endif
}

void print_statistics()
{
    uint32_t uptime_s = (millis() - uptime_start) / 1000;
    float avg_lat = latency_samples > 0 ? (float)total_latency_ms / latency_samples : 0;
    float loss = rx_count > 0 ? (float)rx_invalid / rx_count * 100 : 0;
    float srv_rate = tx_to_server_count > 0 ? (float)tx_to_server_success / tx_to_server_count * 100 : 0;

    DEBUG_PRINTLN("\n=== Gateway Stats ===");
    DEBUG_PRINTF("Uptime: %02u:%02u:%02u\n", uptime_s/3600, (uptime_s%3600)/60, uptime_s%60);
    DEBUG_PRINTF("RX: %u valid, %u invalid (%.1f%% loss)\n", rx_valid, rx_invalid, loss);
    DEBUG_PRINTF("TX: %u/%u success (%.1f%%)\n", tx_to_server_success, tx_to_server_count, srv_rate);
    if (latency_samples > 0) {
        DEBUG_PRINTF("Latency: %.0fms avg, %u-%ums\n", avg_lat, 
                     min_latency_ms == UINT32_MAX ? 0 : min_latency_ms, max_latency_ms);
    }
    DEBUG_PRINTF("Energy: %.2f mAh\n", total_energy_mah);
#if ENABLE_WIFI
    if (wifi_connected) DEBUG_PRINTF("WiFi RSSI: %d dBm\n", WiFi.RSSI());
#endif
    DEBUG_PRINTLN("=====================\n");
}

void update_energy_consumption()
{
    uint32_t now = millis();
    uint32_t elapsed_ms = now - last_energy_calc_time;
    
    if (elapsed_ms >= 1000) {
        float current_ma = wifi_connected ? ACTIVE_CURRENT_MA : IDLE_CURRENT_MA;
        float hours = elapsed_ms / 3600000.0;
        total_energy_mah += current_ma * hours;
        last_energy_calc_time = now;
    }
}

String get_gateway_stats_json()
{
    StaticJsonDocument<512> doc;
    
    doc["type"] = "gateway_stats";
    doc["gateway_id"] = GATEWAY_ID;
    doc["timestamp"] = get_iso8601_timestamp();
    
    uint32_t uptime_seconds = (millis() - uptime_start) / 1000;
    doc["uptime_seconds"] = uptime_seconds;
    
    JsonObject lora = doc["lora_stats"].to<JsonObject>();
    lora["rx_total"] = rx_count;
    lora["rx_valid"] = rx_valid;
    lora["rx_invalid"] = rx_invalid;
    lora["rx_checksum_error"] = rx_checksum_error;
    lora["packet_loss_percent"] = rx_count > 0 ? (float)rx_invalid / rx_count * 100 : 0;
    
    JsonObject server = doc["server_stats"].to<JsonObject>();
    server["tx_total"] = tx_to_server_count;
    server["tx_success"] = tx_to_server_success;
    server["tx_failed"] = tx_to_server_failed;
    server["success_rate_percent"] = tx_to_server_count > 0 ? (float)tx_to_server_success / tx_to_server_count * 100 : 0;
    
    JsonObject latency = doc["latency"].to<JsonObject>();
    latency["avg_ms"] = latency_samples > 0 ? (float)total_latency_ms / latency_samples : 0;
    latency["min_ms"] = min_latency_ms == UINT32_MAX ? 0 : min_latency_ms;
    latency["max_ms"] = max_latency_ms;
    latency["last_ms"] = last_latency_ms;
    latency["samples"] = latency_samples;
    
    doc["energy_mah"] = total_energy_mah;
    
    if (wifi_connected) {
        doc["wifi_rssi"] = WiFi.RSSI();
    }
    
    String json;
    serializeJson(doc, json);
    return json;
}

void send_gateway_stats()
{
#if USE_HTTP && ENABLE_WIFI
    if (!wifi_connected) return;
    
    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + "/api/gateway-stats";
    String stats_json = get_gateway_stats_json();
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(stats_json);
    
    DEBUG_PRINTF("[STATS] Sent (%d)\n", httpCode);
    http.end();
#endif
}

#if ENABLE_BATCH_MODE
void add_to_batch(const String& json)
{
    if (batch_count == 0) {
        batch_start_time = millis();
    }
    
    batch_buffer[batch_count++] = json;
    DEBUG_PRINTF("[BATCH] Added %d/%d\n", batch_count, BATCH_SIZE);
    
    if (batch_count >= BATCH_SIZE) {
        flush_batch();
    }
}

void flush_batch()
{
    if (batch_count == 0) return;
    
    DEBUG_PRINTF("[BATCH] Sending %d messages...\n", batch_count);
    
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.to<JsonArray>();
    
    for (uint8_t i = 0; i < batch_count; i++) {
        StaticJsonDocument<384> item;
        deserializeJson(item, batch_buffer[i]);
        arr.add(item);
    }
    
    String batch_json;
    serializeJson(doc, batch_json);
    
    forward_to_server(batch_json.c_str());
    
    batch_count = 0;
    batch_start_time = 0;
}
#endif

#if TEST_MODE
void send_test_packet()
{
    DEBUG_PRINTLN("\n[TEST] Simulating LoRa packet...");
    
    SensorDataMessage test_msg;
    test_msg.msg_type = MSG_TYPE_SENSOR_DATA;
    test_msg.client_id = 42;
    test_msg.timestamp = millis() / 1000;
    test_msg.humidity = random(3000, 8000);
    test_msg.distance_cm = random(5, 200);
    test_msg.temperature = 0;
    test_msg.battery = random(60, 100);
    test_msg.reserved = 0;
    
    uint8_t* data = (uint8_t*)&test_msg;
    uint8_t checksum = 0;
    for (size_t i = 0; i < sizeof(SensorDataMessage) - 1; i++) {
        checksum ^= data[i];
    }
    test_msg.checksum = checksum;
    
    float fake_rssi = random(-90, -30) / 1.0;
    float fake_snr = random(5, 12) / 1.0;
    
    rx_count++;
    last_rx_time = millis();
    
    DEBUG_PRINTF("[TEST] Humidity=%.1f%%, Distance=%dcm, Presence=%s\n",
                 test_msg.humidity / 100.0,
                 test_msg.distance_cm,
                 (test_msg.distance_cm < 100) ? "YES" : "No");
    
    process_message((uint8_t*)&test_msg, sizeof(SensorDataMessage), fake_rssi, fake_snr);
    
    send_gateway_stats();
}
#endif
