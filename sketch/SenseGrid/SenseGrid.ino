#include <Arduino.h>

// Protótipos C para manter a ideia do "extern_*"
extern "C" void sg_core_init(void);
extern "C" void sg_adapters_logger_init(void);

void setup() {
  Serial.begin(115200);
  while (!Serial) { /* ESP32-S3 CDC: pode precisar de um tempinho */ }
  Serial.println("[SenseGrid] boot");

  sg_adapters_logger_init();
  sg_core_init();

  Serial.println("[SenseGrid] init OK");
}

void loop() {
  delay(1000);
}
