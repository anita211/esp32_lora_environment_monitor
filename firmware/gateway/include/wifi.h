#ifndef WIFI_H
#define WIFI_H

#include <Arduino.h>


extern bool wifi_connected;
extern bool time_synced;

struct server_stats_t {
    uint32_t total;
    uint32_t success;
    uint32_t failed;
};

struct latency_t {
    uint32_t total_ms;
    uint32_t samples;
    uint32_t min_ms;
    uint32_t max_ms;
    uint32_t last_ms;
};

extern struct server_stats_t server_stats;
extern struct latency_t latency;

void init_wifi();
void check_wifi_connection();
int get_current_wifi_rssi();
void forward_to_server(const char* json_data);
void send_gateway_statistics();
String build_gateway_stats_json();
String get_iso8601_timestamp();

#endif
