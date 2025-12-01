#include "batch.h"
#include "utils.h"
#include "protocol.h"
#include "wifi.h"
#include <ArduinoJson.h>



#ifdef BATCH_ON
String batch_buffer[BATCH_SIZE];
uint8_t batch_count = 0;
uint32_t batch_start_time = 0;

void add_to_batch(const String& json) {
    if (batch_count == 0) {
        batch_start_time = millis();
    }

    batch_buffer[batch_count++] = json;
    print_log("Adding messages to batch: %d/%d\n", batch_count, BATCH_SIZE);

    if (batch_count >= BATCH_SIZE) {
        flush_batch();
    }
}

void flush_batch() {
    if (batch_count == 0) return;

    print_log("Flushing batch: %d\n", batch_count);

    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.to<JsonArray>();

    for (uint8_t i = 0; i < batch_count; i++) {
        StaticJsonDocument<384> item;
        deserializeJson(item, batch_buffer[i]);
        arr.add(item);
    }

    String batch_json;
    serializeJson(doc, batch_json);

    forward_to_server(batch_json.c_str());

    batch_count = 0;
    batch_start_time = 0;
}
#endif
