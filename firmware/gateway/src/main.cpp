/**
 * @file main.cpp
 * @brief ESP32 LoRa Environment Monitor - Gateway Node
 * 
 * This firmware receives sensor data via LoRa from client nodes
 * and forwards it to a server via WiFi HTTP requests.
 * 
 * Features:
 *   - LoRa reception with RSSI/SNR monitoring
 *   - WiFi connectivity with NTP time sync
 *   - HTTP forwarding with batch mode support
 *   - Statistics tracking and reporting
 *   - Test mode for development without hardware
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <RadioLib.h>
#include <time.h>
#include <sys/time.h>

#include "config.h"
#include "protocol.h"

#if WIFI_ENABLED
#include <HTTPClient.h>
#include <WiFi.h>
#endif

/* ============================================================================
 * GLOBAL STATE
 * ============================================================================ */

// LoRa radio instance
static SX1262 g_radio = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_RST, PIN_LORA_BUSY);

// Reception statistics
static struct {
    uint32_t total;
    uint32_t valid;
    uint32_t invalid;
    uint32_t checksum_errors;
} g_rx_stats = {0, 0, 0, 0};

// Server transmission statistics
static struct {
    uint32_t total;
    uint32_t success;
    uint32_t failed;
} g_server_stats = {0, 0, 0};

// Latency tracking
static struct {
    uint32_t total_ms;
    uint32_t samples;
    uint32_t min_ms;
    uint32_t max_ms;
    uint32_t last_ms;
} g_latency = {0, 0, UINT32_MAX, 0, 0};

// Energy consumption tracking
static struct {
    uint32_t start_time;
    float total_mah;
    uint32_t last_calc_time;
} g_energy = {0, 0.0f, 0};

// Current consumption estimates (mA)
static const float CURRENT_ACTIVE_MA = 120.0f;
static const float CURRENT_WIFI_TX_MA = 220.0f;
static const float CURRENT_IDLE_MA = 20.0f;

// Timing
static uint32_t g_last_stats_time = 0;
static uint32_t g_last_rx_time = 0;

#if TEST_MODE_ENABLED
static uint32_t g_last_test_time = 0;
#endif

// Connection state
static bool g_wifi_connected = false;
static bool g_time_synced = false;

// Reception buffer
static uint8_t g_rx_buffer[MAX_PACKET_SIZE];

// Batch mode buffer
#if BATCH_MODE_ENABLED
static String g_batch_buffer[BATCH_SIZE];
static uint8_t g_batch_count = 0;
static uint32_t g_batch_start_time = 0;
#endif

/* ============================================================================
 * FUNCTION DECLARATIONS
 * ============================================================================ */

// Initialization
static void init_lora_radio();
static void init_wifi();

// LoRa reception
static void listen_for_messages();
static void process_received_message(uint8_t* data, size_t length, float rssi, float snr);
static void handle_sensor_data(SensorDataMessage* msg, float rssi, float snr);

// Server communication
static void forward_to_server(const char* json_data);
static void send_gateway_statistics();
static String build_sensor_json(SensorDataMessage* msg, float rssi, float snr);
static String build_gateway_stats_json();

// Batch mode
#if BATCH_MODE_ENABLED
static void add_to_batch(const String& json);
static void flush_batch();
#endif

// Utilities
static String get_iso8601_timestamp();
static void update_energy_consumption();
static void print_statistics();

// Test mode
#if TEST_MODE_ENABLED
static void generate_test_packet();
#endif

/* ============================================================================
 * SETUP
 * ============================================================================ */

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);
    delay(500);

    LOG_F("\n========================================\n");
    LOG_F("[GATEWAY %d] Starting...\n", GATEWAY_ID);
    LOG_F("========================================\n");

    g_energy.start_time = millis();
    g_energy.last_calc_time = millis();

    init_lora_radio();

#if WIFI_ENABLED
    init_wifi();
#endif

    LOG_LN("[GATEWAY] Ready and listening\n");
    g_last_stats_time = millis();
}

/* ============================================================================
 * MAIN LOOP
 * ============================================================================ */

void loop()
{
#if TEST_MODE_ENABLED
    // Test mode: generate simulated packets
    if (millis() - g_last_test_time >= TEST_INTERVAL_MS) {
        generate_test_packet();
        g_last_test_time = millis();
    }
#else
    // Normal mode: listen for LoRa messages
    listen_for_messages();
#endif

    update_energy_consumption();

#if BATCH_MODE_ENABLED
    // Flush batch on timeout
    if (g_batch_count > 0 && (millis() - g_batch_start_time >= BATCH_TIMEOUT_MS)) {
        LOG_LN("[BATCH] Timeout reached, flushing...");
        flush_batch();
    }
#endif

    // Periodic statistics report
    if (millis() - g_last_stats_time >= STATS_INTERVAL_MS) {
        print_statistics();
        send_gateway_statistics();
        g_last_stats_time = millis();
    }

#if WIFI_ENABLED
    // WiFi reconnection
    if (!WiFi.isConnected() && g_wifi_connected) {
        LOG_LN("[WIFI] Disconnected! Reconnecting...");
        g_wifi_connected = false;
        init_wifi();
    }
#endif
}

/* ============================================================================
 * LORA INITIALIZATION
 * ============================================================================ */

static void init_lora_radio()
{
    LOG_F("[LORA] Initializing: %.1f MHz, BW=%.0f kHz, SF=%d\n",
          LORA_FREQUENCY_MHZ, LORA_BANDWIDTH_KHZ, LORA_SPREADING_FACTOR);

    int result = g_radio.begin(
        LORA_FREQUENCY_MHZ,
        LORA_BANDWIDTH_KHZ,
        LORA_SPREADING_FACTOR,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        10,  // TX power (not used for RX)
        LORA_PREAMBLE_LENGTH
    );

    if (result == RADIOLIB_ERR_NONE) {
        LOG_LN("[LORA] Initialized successfully");
        g_radio.startReceive();
    } else {
        LOG_F("[LORA] FAILED to initialize (error %d)\n", result);
        while (true) delay(1000);  // Halt on error
    }
}

/* ============================================================================
 * WIFI INITIALIZATION
 * ============================================================================ */

static void init_wifi()
{
#if WIFI_ENABLED
    LOG_F("[WIFI] Connecting to %s...\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_TIMEOUT_MS) {
        delay(500);
    }

    if (WiFi.status() == WL_CONNECTED) {
        g_wifi_connected = true;
        LOG_F("[WIFI] Connected - IP: %s\n", WiFi.localIP().toString().c_str());

        // Sync time via NTP
        configTime(-3 * 3600, 0, "pool.ntp.org");  // UTC-3 (Brazil)
        int retry = 0;
        while (time(nullptr) < 100000 && retry < 10) {
            delay(500);
            retry++;
        }
        g_time_synced = (time(nullptr) > 100000);
        LOG_F("[NTP] Time sync: %s\n", g_time_synced ? "OK" : "FAILED");
    } else {
        g_wifi_connected = false;
        LOG_LN("[WIFI] Connection FAILED");
    }
#endif
}

/* ============================================================================
 * LORA RECEPTION
 * ============================================================================ */

static void listen_for_messages()
{
    if (g_radio.getPacketLength() > 0) {
        int state = g_radio.readData(g_rx_buffer, MAX_PACKET_SIZE);

        if (state == RADIOLIB_ERR_NONE) {
            size_t length = g_radio.getPacketLength();
            float rssi = g_radio.getRSSI();
            float snr = g_radio.getSNR();

            g_rx_stats.total++;
            g_last_rx_time = millis();

            LOG_F("[RX] Packet #%u: %d bytes, RSSI=%.0f dBm, SNR=%.1f dB\n",
                  g_rx_stats.total, length, rssi, snr);

            process_received_message(g_rx_buffer, length, rssi, snr);
        } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
            LOG_LN("[RX] CRC error - packet discarded");
            g_rx_stats.invalid++;
        }

        g_radio.startReceive();
    }
}

/* ============================================================================
 * MESSAGE PROCESSING
 * ============================================================================ */

static void process_received_message(uint8_t* data, size_t length, float rssi, float snr)
{
    if (length < 1) {
        g_rx_stats.invalid++;
        return;
    }

    uint8_t msg_type = data[0];

    switch (msg_type) {
    case MSG_TYPE_SENSOR_DATA:
        if (length == sizeof(SensorDataMessage)) {
            if (verify_checksum(data, length)) {
                handle_sensor_data(reinterpret_cast<SensorDataMessage*>(data), rssi, snr);
                g_rx_stats.valid++;
            } else {
                LOG_LN("[RX] Checksum verification failed");
                g_rx_stats.checksum_errors++;
                g_rx_stats.invalid++;
            }
        } else {
            LOG_F("[RX] Invalid sensor data length: %d (expected %d)\n",
                  length, sizeof(SensorDataMessage));
            g_rx_stats.invalid++;
        }
        break;

    case MSG_TYPE_HEARTBEAT:
        if (length == sizeof(HeartbeatMessage) && verify_checksum(data, length)) {
            HeartbeatMessage* hb = reinterpret_cast<HeartbeatMessage*>(data);
            LOG_F("[RX] Heartbeat from Node %d (status: 0x%02X)\n",
                  hb->client_id, hb->status);
            g_rx_stats.valid++;
        } else {
            g_rx_stats.invalid++;
        }
        break;

    case MSG_TYPE_ALERT:
        if (length == sizeof(AlertMessage) && verify_checksum(data, length)) {
            AlertMessage* alert = reinterpret_cast<AlertMessage*>(data);
            LOG_F("[RX] ALERT from Node %d: code=0x%02X, value=%d, severity=%d\n",
                  alert->client_id, alert->alert_code, alert->alert_value, alert->severity);
            g_rx_stats.valid++;
        } else {
            g_rx_stats.invalid++;
        }
        break;

    default:
        LOG_F("[RX] Unknown message type: 0x%02X\n", msg_type);
        g_rx_stats.invalid++;
        break;
    }
}

/* ============================================================================
 * SENSOR DATA HANDLING
 * ============================================================================ */

static void handle_sensor_data(SensorDataMessage* msg, float rssi, float snr)
{
    const float humidity = decode_humidity(msg->humidity);
    const uint16_t distance = msg->distance_cm;
    const bool presence = distance < 100;

    LOG_F("[DATA] Node %d: Moisture=%.1f%%, Distance=%dcm, Presence=%s, Battery=%d%%\n",
          msg->client_id, humidity, distance, presence ? "YES" : "No", msg->battery);

    String json = build_sensor_json(msg, rssi, snr);

#if HTTP_ENABLED && WIFI_ENABLED
    if (g_wifi_connected) {
#if BATCH_MODE_ENABLED
        add_to_batch(json);
#else
        forward_to_server(json.c_str());
#endif
    }
#endif
}

/* ============================================================================
 * JSON BUILDING
 * ============================================================================ */

static String get_iso8601_timestamp()
{
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

static String build_sensor_json(SensorDataMessage* msg, float rssi, float snr)
{
    StaticJsonDocument<384> doc;

    // Node identification
    char node_id[20];
    snprintf(node_id, sizeof(node_id), "node-%d", msg->client_id);
    doc["node_id"] = node_id;
    doc["gateway_id"] = GATEWAY_ID;

    // Timestamps
    doc["timestamp"] = get_iso8601_timestamp();
    doc["client_timestamp"] = msg->timestamp;

    // Sensor data
    const float humidity = decode_humidity(msg->humidity);
    const uint16_t distance = msg->distance_cm;
    const bool presence = distance < 100;

    JsonObject sensors = doc["sensors"].to<JsonObject>();
    sensors["humidity_percent"] = humidity;
    sensors["distance_cm"] = distance;
    sensors["presence_detected"] = presence;

    // Battery
    doc["battery_percent"] = msg->battery;

    // Radio metrics
    JsonObject radio = doc["radio"].to<JsonObject>();
    radio["rssi_dbm"] = rssi;
    radio["snr_db"] = snr;

    String json;
    serializeJson(doc, json);
    return json;
}

static String build_gateway_stats_json()
{
    StaticJsonDocument<512> doc;

    doc["type"] = "gateway_stats";
    doc["gateway_id"] = GATEWAY_ID;
    doc["timestamp"] = get_iso8601_timestamp();

    uint32_t uptime_seconds = (millis() - g_energy.start_time) / 1000;
    doc["uptime_seconds"] = uptime_seconds;

    // LoRa statistics
    JsonObject lora = doc["lora_stats"].to<JsonObject>();
    lora["rx_total"] = g_rx_stats.total;
    lora["rx_valid"] = g_rx_stats.valid;
    lora["rx_invalid"] = g_rx_stats.invalid;
    lora["rx_checksum_error"] = g_rx_stats.checksum_errors;
    lora["packet_loss_percent"] = g_rx_stats.total > 0
        ? (static_cast<float>(g_rx_stats.invalid) / g_rx_stats.total * 100.0f) : 0.0f;

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

/* ============================================================================
 * SERVER COMMUNICATION
 * ============================================================================ */

static void forward_to_server(const char* json_data)
{
#if HTTP_ENABLED && WIFI_ENABLED
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
            LOG_F("[HTTP] POST success (%u ms)\n", g_latency.last_ms);
        } else {
            g_server_stats.failed++;
            LOG_F("[HTTP] Error response: %d\n", http_code);
        }
    } else {
        g_server_stats.failed++;
        LOG_F("[HTTP] Request failed: %s\n", http.errorToString(http_code).c_str());
    }

    http.end();
#endif
}

static void send_gateway_statistics()
{
#if HTTP_ENABLED && WIFI_ENABLED
    if (!g_wifi_connected) return;

    HTTPClient http;
    String url = String("http://") + SERVER_HOST + ":" + SERVER_PORT + SERVER_ENDPOINT_STATS;
    String stats_json = build_gateway_stats_json();

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int http_code = http.POST(stats_json);

    LOG_F("[STATS] Sent to server (response: %d)\n", http_code);
    http.end();
#endif
}

/* ============================================================================
 * BATCH MODE
 * ============================================================================ */

#if BATCH_MODE_ENABLED
static void add_to_batch(const String& json)
{
    if (g_batch_count == 0) {
        g_batch_start_time = millis();
    }

    g_batch_buffer[g_batch_count++] = json;
    LOG_F("[BATCH] Added message %d/%d\n", g_batch_count, BATCH_SIZE);

    if (g_batch_count >= BATCH_SIZE) {
        flush_batch();
    }
}

static void flush_batch()
{
    if (g_batch_count == 0) return;

    LOG_F("[BATCH] Flushing %d messages...\n", g_batch_count);

    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.to<JsonArray>();

    for (uint8_t i = 0; i < g_batch_count; i++) {
        StaticJsonDocument<384> item;
        deserializeJson(item, g_batch_buffer[i]);
        arr.add(item);
    }

    String batch_json;
    serializeJson(doc, batch_json);

    forward_to_server(batch_json.c_str());

    g_batch_count = 0;
    g_batch_start_time = 0;
}
#endif

/* ============================================================================
 * STATISTICS & MONITORING
 * ============================================================================ */

static void update_energy_consumption()
{
    uint32_t now = millis();
    uint32_t elapsed_ms = now - g_energy.last_calc_time;

    if (elapsed_ms >= 1000) {
        float current_ma = g_wifi_connected ? CURRENT_ACTIVE_MA : CURRENT_IDLE_MA;
        float hours = elapsed_ms / 3600000.0f;
        g_energy.total_mah += current_ma * hours;
        g_energy.last_calc_time = now;
    }
}

static void print_statistics()
{
    uint32_t uptime_s = (millis() - g_energy.start_time) / 1000;
    float avg_latency = g_latency.samples > 0
        ? (static_cast<float>(g_latency.total_ms) / g_latency.samples) : 0.0f;
    float packet_loss = g_rx_stats.total > 0
        ? (static_cast<float>(g_rx_stats.invalid) / g_rx_stats.total * 100.0f) : 0.0f;
    float server_success = g_server_stats.total > 0
        ? (static_cast<float>(g_server_stats.success) / g_server_stats.total * 100.0f) : 0.0f;

    LOG_LN("\n========== Gateway Statistics ==========");
    LOG_F("Uptime: %02u:%02u:%02u\n", uptime_s / 3600, (uptime_s % 3600) / 60, uptime_s % 60);
    LOG_F("LoRa RX: %u valid, %u invalid (%.1f%% loss)\n",
          g_rx_stats.valid, g_rx_stats.invalid, packet_loss);
    LOG_F("Server TX: %u/%u success (%.1f%%)\n",
          g_server_stats.success, g_server_stats.total, server_success);
    if (g_latency.samples > 0) {
        LOG_F("Latency: %.0f ms avg, %u-%u ms range\n", avg_latency,
              g_latency.min_ms == UINT32_MAX ? 0 : g_latency.min_ms, g_latency.max_ms);
    }
    LOG_F("Energy: %.2f mAh\n", g_energy.total_mah);
#if WIFI_ENABLED
    if (g_wifi_connected) {
        LOG_F("WiFi RSSI: %d dBm\n", WiFi.RSSI());
    }
#endif
    LOG_LN("=========================================\n");
}

/* ============================================================================
 * TEST MODE
 * ============================================================================ */

#if TEST_MODE_ENABLED
static void generate_test_packet()
{
    LOG_LN("\n[TEST] Generating simulated sensor packet...");

    SensorDataMessage test_msg;
    test_msg.msg_type = MSG_TYPE_SENSOR_DATA;
    test_msg.client_id = 42;  // Test node ID
    test_msg.timestamp = millis() / 1000;
    test_msg.humidity = random(3000, 8000);      // 30-80%
    test_msg.distance_cm = random(5, 200);
    test_msg.temperature = 0;
    test_msg.battery = random(60, 100);
    test_msg.reserved = 0;

    // Calculate checksum
    uint8_t* data = reinterpret_cast<uint8_t*>(&test_msg);
    test_msg.checksum = calculate_checksum(data, sizeof(SensorDataMessage));

    // Simulate radio metrics
    float fake_rssi = static_cast<float>(random(-90, -30));
    float fake_snr = static_cast<float>(random(5, 12));

    g_rx_stats.total++;
    g_last_rx_time = millis();

    LOG_F("[TEST] Moisture=%.1f%%, Distance=%dcm, Presence=%s\n",
          test_msg.humidity / 100.0f,
          test_msg.distance_cm,
          (test_msg.distance_cm < 100) ? "YES" : "No");

    process_received_message(reinterpret_cast<uint8_t*>(&test_msg),
                             sizeof(SensorDataMessage), fake_rssi, fake_snr);

    send_gateway_statistics();
}
#endif
