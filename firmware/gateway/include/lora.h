#ifndef LORA_H
#define LORA_H

#include <RadioLib.h>
#include "constants.h"
#include "message_struct.h"

class LoRaRadio {
  public:
    class Stats {
      public:
        Stats() :
          total_rx_packets(0),
          total_rx_valids(0),
          total_rx_invalids(0),
          total_checksum_errors(0) {}

        uint32_t total_rx_packets;
        uint32_t total_rx_valids;
        uint32_t total_rx_invalids;
        uint32_t total_checksum_errors;
    };

    static LoRaRadio& get_instance();
    void setup();
    void check_packets();
    Stats get_stats() const {
        return stats;
    }
    uint32_t get_last_rx_time_ms() const {
        return last_rx_time_ms;
    }

  private:
    LoRaRadio();
    static LoRaRadio* loRaRadio;
    
    SX1262 lora_handler;
    
    Stats stats;
    uint32_t last_rx_time_ms;
    
    uint8_t packet_rx_buffer[LORA_MAX_PACKET_SIZE];
};

#endif // LORA_H
