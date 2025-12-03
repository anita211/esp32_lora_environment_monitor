#include "lora.h"
#include "utils.h"
#include "batch.h"
#include "processing.h"
#include "wifi.h"
#include <SPI.h>

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
  
  // Hardware reset do módulo LoRa
  pinMode(LORA_PIN_RST, OUTPUT);
  digitalWrite(LORA_PIN_RST, LOW);
  delay(10);
  digitalWrite(LORA_PIN_RST, HIGH);
  delay(10);

  // Inicialização do SPI
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
  print_log(
    "Lora setup - %.1f MHz | SF=%d | BW=%.0f kHz\n",
    LORA_FREQUENCY_MHZ,
    LORA_SPREADING_FACTOR,
    LORA_BANDWIDTH_KHZ
  );

  if (status_code == RADIOLIB_ERR_NONE) {
      lora_handler.setCurrentLimit(140);
      lora_handler.setCRC(true);  // Habilita verificação de CRC
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
    // Verifica se há um novo pacote usando o status de interrupção
    // Isso evita ler pacotes duplicados ou dados inválidos
    int irq = lora_handler.getIrqStatus();
    if (!(irq & RADIOLIB_SX126X_IRQ_RX_DONE)) {
        return;
    }

    size_t packet_size = lora_handler.getPacketLength();
    
    // Ignora pacotes vazios ou muito grandes
    if (packet_size == 0 || packet_size > LORA_MAX_PACKET_SIZE) {
        lora_handler.startReceive();
        return;
    }

    int state = lora_handler.readData(packet_rx_buffer, packet_size);
    float rssi = lora_handler.getRSSI();
    float snr = lora_handler.getSNR();

    // Reinicia recepção imediatamente após ler o pacote
    lora_handler.startReceive();

    // Verifica erros de CRC
    if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        print_log("Lora packet RX CRC error - packet discarded\n");
        stats.total_rx_invalids++;
        return;
    }

    // Verifica outros erros
    if (state != RADIOLIB_ERR_NONE) {
        return;
    }

    // Filtra pacotes com RSSI muito alto (provavelmente ruído/interferência)
    if (rssi > -20) {
        return;
    }

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
}
