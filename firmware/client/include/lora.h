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
          total_tx_packets(0),
          total_tx_success(0),
          total_tx_failed(0),
          total_tx_skipped(0) {}

        uint32_t total_tx_packets;
        uint32_t total_tx_success;
        uint32_t total_tx_failed;
        uint32_t total_tx_skipped;
    };

    static LoRaRadio& get_instance();
    void setup();
    bool transmit(const uint8_t* data, size_t length);
    void increment_skipped();
    void increment_total();
    bool is_ready() const {
        return ready;
    }
    Stats get_stats() const {
        return stats;
    }

  private:
    LoRaRadio();
    static LoRaRadio* loRaRadio;
    
    SX1262 lora_handler;
    
    Stats stats;
    bool ready;
};

#endif // LORA_H
