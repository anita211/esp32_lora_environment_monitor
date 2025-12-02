#include "processing.h"
#include "lora.h"
#include "wifi.h"
#include "utils.h"
#include "batch.h"
#include <ArduinoJson.h>


void process_rx_lora_message(
    uint8_t* data,
    size_t length,
    float rssi,
    float snr
) {
    LoRaRadio& radio = LoRaRadio::get_instance();
    LoRaRadio::Stats& stats = radio.get_stats();  // Use reference to modify original

    if (length < 1) {
        stats.total_rx_invalids++;
        return;
    }

    uint8_t msg_type = data[0];

    switch (msg_type) {
    case MSG_TYPE_SENSOR_DATA:
        if (length == sizeof(SensorDataMessage)) {
            if (verify_checksum(data, length)) {
                handle_sensor_data(
                  reinterpret_cast<SensorDataMessage*>(data),
                  rssi,
                  snr
                );
                stats.total_rx_valids++;
            } else {
                print_log("Lora packet RX checksum error - packet discarded\n");
                stats.total_checksum_errors++;
                stats.total_rx_invalids++;
            }
        } else {
            print_log(
              "Lora packet RX invalid sensor data length: %d (expected %d)\n",
              length,
              sizeof(SensorDataMessage)
            );
            stats.total_rx_invalids++;
        }
        break;

    case MSG_TYPE_HEARTBEAT:
        if (length == sizeof(HeartbeatMessage) && verify_checksum(data, length)) {
            HeartbeatMessage* hb = reinterpret_cast<HeartbeatMessage*>(data);
            print_log(
              "Lora packet RX - Heartbeat from Node %d (status: 0x%02X)\n",
              hb->client_id,
              hb->status
            );
            stats.total_rx_valids++;
        } else {
            stats.total_rx_invalids++;
        }
        break;

    case MSG_TYPE_ALERT:
        if (length == sizeof(AlertMessage) && verify_checksum(data, length)) {
            AlertMessage* alert = reinterpret_cast<AlertMessage*>(data);
            print_log(
              "Lora packet RX - ALERT from Node %d: code=0x%02X | value=%d | severity=%d\n",
              alert->client_id,
              alert->alert_code,
              alert->alert_value,
              alert->severity
            );
            stats.total_rx_valids++;
        } else {
            stats.total_rx_invalids++;
        }
        break;

    default:
        print_log("Lora packet RX - Unknown message type: 0x%02X\n", msg_type);
        stats.total_rx_invalids++;
        break;
    }
}

static String build_sensor_json(SensorDataMessage* msg, float rssi, float snr) {
    StaticJsonDocument<512> doc;

    // Node identification
    char node_id[20];
    snprintf(node_id, sizeof(node_id), "node-%d", msg->client_id);
    doc["node_id"] = node_id;
    doc["NODE_ID"] = NODE_ID;

    // Timestamps
    doc["timestamp"] = get_iso8601_timestamp();
    doc["client_timestamp"] = msg->timestamp;

    // Sensor data
    const float temperature = decode_temperature(msg->temperature);
    const float humidity = decode_humidity(msg->humidity);
    const uint16_t distance = msg->distance_cm;
    const uint16_t luminosity = msg->luminosity_lux;
    const bool presence = distance < 100;

    JsonObject sensors = doc["sensors"].to<JsonObject>();
    sensors["temperature_celsius"] = temperature;
    sensors["humidity_percent"] = humidity;
    sensors["distance_cm"] = distance;
    sensors["luminosity_lux"] = luminosity;
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

void handle_sensor_data(SensorDataMessage* msg, float rssi, float snr) {
    const float temperature = decode_temperature(msg->temperature);
    const float humidity = decode_humidity(msg->humidity);
    const uint16_t distance = msg->distance_cm;
    const uint16_t luminosity = msg->luminosity_lux;
    const bool presence = distance < MAX_DISTANCE_TO_BE_PRESENCE_CM;

    print_log(
      "Lora Data handling - Node %d: Temp=%.1f°C | Moisture=%.1f%% | Distance=%dcm | Lux=%u | Presence=%s | Battery=%d%%\n",
      msg->client_id,
      temperature,
      humidity,
      distance,
      luminosity,
      presence ? "YES" : "No",
      msg->battery
    );

    String json = build_sensor_json(msg, rssi, snr);

#ifdef WIFI_ON
    if (wifi_connected) {
#ifdef BATCH_ON
        add_to_batch(json);
#else
        forward_to_server(json.c_str());
#endif
    }
#endif
}
