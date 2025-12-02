#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>
#include <BH1750.h>
#include <VL53L0X.h>
#include <Adafruit_AHTX0.h>

class Sensors {
  public:
    static Sensors& get_instance();
    void setup();
    void read_all(float& out_humidity, float& out_distance, float& out_temperature, uint16_t& out_luminosity);
    
    float get_prev_humidity() const { return prev_humidity; }
    float get_prev_distance() const { return prev_distance; }
    void set_prev_humidity(float value) { prev_humidity = value; }
    void set_prev_distance(float value) { prev_distance = value; }

  private:
    Sensors();
    static Sensors* instance;
    
    BH1750 lux_sensor;
    VL53L0X distance_sensor;
    Adafruit_AHTX0 aht;
    float prev_humidity;
    float prev_distance;
    
    // Sensor initialization status flags
    bool bh1750_initialized;
    bool vl53l0x_initialized;
    bool aht10_initialized;
    
    float read_humidity();
    float read_distance();
    float read_temperature();
    uint16_t read_luminosity();
    float generate_simulated_value(float base_value, float variation);
};

#endif // SENSORS_H
