#ifndef ENERGY_MANAGER_H
#define ENERGY_MANAGER_H

#include <Arduino.h>

struct energy_t {
    uint32_t start_time;
    float total_mah;
    uint32_t last_calc_time;
};
extern struct energy_t energy;

static const float CURRENT_ACTIVE_MA = 120.0f;
static const float CURRENT_WIFI_TX_MA = 220.0f;
static const float CURRENT_IDLE_MA = 20.0f;

void update_energy_consumption();

#endif // ENERGY_MANAGER_H
