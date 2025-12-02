#include "lora.h"
#include "utils.h"

LoRaRadio* LoRaRadio::loRaRadio = nullptr;

LoRaRadio& LoRaRadio::get_instance() {
    if (loRaRadio == nullptr) {
        loRaRadio = new LoRaRadio();
    }
    return *loRaRadio;
}

LoRaRadio::LoRaRadio() :
    lora_handler(new Module(
      LORA_PIN_CS, LORA_PIN_IRQ, LORA_PIN_RST, LORA_PIN_GPIO_INT
    )),
    stats(),
    ready(false) {}

void LoRaRadio::setup() {
  print_log("Initializing LoRa radio at %.1f MHz, SF%d, %d dBm\n", 
        LORA_FREQUENCY_MHZ, LORA_SPREADING_FACTOR, LORA_TX_POWER);

  // Hardware reset
  pinMode(LORA_PIN_RST, OUTPUT);
  digitalWrite(LORA_PIN_RST, LOW);
  delay(10);
  digitalWrite(LORA_PIN_RST, HIGH);
  delay(10);

  // Initialize SPI
  SPI.begin(LORA_PIN_SCK, LORA_PIN_MISO, LORA_PIN_MOSI, LORA_PIN_CS);
  SPI.setFrequency(2000000);
  delay(100);

  int status_code = lora_handler.begin(
      LORA_FREQUENCY_MHZ,
      LORA_BANDWIDTH_KHZ,
      LORA_SPREADING_FACTOR,
      LORA_CODING_RATE,
      LORA_SYNC_WORD,
      LORA_TX_POWER,
      LORA_PREAMBLE_LENGTH
  );

  if (status_code == RADIOLIB_ERR_NONE) {
      ready = true;
      lora_handler.setCurrentLimit(140);
      print_log("LoRa radio initialized successfully\n");
  } else {
      ready = false;
      print_log("LoRa radio initialization failed with error %d\n", status_code);
  }
}

bool LoRaRadio::transmit(const uint8_t* data, size_t length) {
    if (!ready) {
        print_log("Transmission error: LoRa radio not initialized\n");
        return false;
    }

    for (int attempt = 1; attempt <= TX_MAX_RETRIES; attempt++) {
        int result = lora_handler.transmit(const_cast<uint8_t*>(data), length);
        
        if (result == RADIOLIB_ERR_NONE) {
            print_log("Transmission successful\n");
            stats.total_tx_success++;
            return true;
        }

        print_log("Transmission attempt %d failed with error %d\n", attempt, result);
        delay(100);
    }

    print_log("All transmission attempts failed\n");
    stats.total_tx_failed++;
    return false;
}

void LoRaRadio::increment_skipped() {
    stats.total_tx_skipped++;
}

void LoRaRadio::increment_total() {
    stats.total_tx_packets++;
}
