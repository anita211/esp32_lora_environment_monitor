#include "energy_manager.h"
#include "wifi.h"
#include "utils.h"

struct g_energy_t g_energy = {0, 0.0f, 0};

void update_energy_consumption() {
    uint32_t now = millis();
    uint32_t elapsed_ms = now - g_energy.last_calc_time;

    if (elapsed_ms >= 1000) {
        float current_ma = g_wifi_connected ? CURRENT_ACTIVE_MA : CURRENT_IDLE_MA;
        float hours = elapsed_ms / 3600000.0f;
        g_energy.total_mah += current_ma * hours;
        g_energy.last_calc_time = now;
    }
}
