#include "energy_manager.h"
#include "wifi.h"
#include "utils.h"

struct energy_t energy = {0, 0.0f, 0};

void update_energy_consumption() {
    uint32_t now = millis();
    uint32_t elapsed_ms = now - energy.last_calc_time;

    if (elapsed_ms >= 1000) {
        float current_ma = wifi_connected ? CURRENT_ACTIVE_MA : CURRENT_IDLE_MA;
        float hours = elapsed_ms / 3600000.0f;
        energy.total_mah += current_ma * hours;
        energy.last_calc_time = now;
    }
}
