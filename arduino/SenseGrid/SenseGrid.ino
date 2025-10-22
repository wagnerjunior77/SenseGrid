#include <Arduino.h>
#include "sg_core.h"
#include "sg_adapters_logger.h"

extern "C" void sg_core_init(void);
extern "C" void sg_adapters_logger_init(void);

void setup() {
  Serial.begin(115200);
  while(!Serial){ delay(10); }
  Serial.println("[SenseGrid] Arduino bridge boot");
  sg_adapters_logger_init();
  sg_core_init();
  Serial.println("[SenseGrid] setup done");
}

void loop() {
  delay(100);
}
