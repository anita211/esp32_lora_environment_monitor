#include "lora.h"
#include "utils.h"
#include "batch.h"
#include "processing.h"
#include "wifi.h"

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
    packet_rx_buffer{0},
    last_rx_time_ms(0) {
      last_rx_time_ms = millis();
    }

void LoRaRadio::setup() {
  print_log("Setting up LoRa radio...\n");
  int status_code = lora_handler.begin(
      LORA_FREQUENCY_MHZ,
      LORA_BANDWIDTH_KHZ,
      LORA_SPREADING_FACTOR,
      LORA_CODING_RATE,
      LORA_SYNC_WORD,
      LORA_TX_POWER,
      LORA_PREAMBLE_LENGTH
  );
  print_log(
    "Lora setup - %.1f MHz | SF=%d | BW=%.0f kHz\n",
    LORA_FREQUENCY_MHZ,
    LORA_SPREADING_FACTOR,
    LORA_BANDWIDTH_KHZ
  );

  if (status_code == RADIOLIB_ERR_NONE) {
      print_log("Lora setup completed\n");
      lora_handler.startReceive();
  } else {
    while (true) {
        print_log("Lora error %d\n", status_code);
        delay(1000);
      }
  }
}

void LoRaRadio::check_packets() {
    int packet_size = lora_handler.getPacketLength();
    
    if (packet_size > 0) {
        int state = lora_handler.readData(packet_rx_buffer, LORA_MAX_PACKET_SIZE);

        if (state == RADIOLIB_ERR_NONE) {
          float rssi = lora_handler.getRSSI();
          float snr = lora_handler.getSNR();

          stats.total_rx_packets++;
          last_rx_time_ms = millis();

          print_log(
            "Received packet - #%u: %d bytes | RSSI=%.0f dBm | SNR=%.1f dB\n",
            stats.total_rx_packets,
            packet_size,
            rssi,
            snr
          );

          process_rx_lora_message(packet_rx_buffer, packet_size, rssi, snr);
        } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
          print_log("Lora packet RX CRC error - packet discarded\n");
          stats.total_rx_invalids++;
        }

        // Limpa o buffer e reinicia recepção para o próximo pacote
        lora_handler.startReceive();
    }
}
