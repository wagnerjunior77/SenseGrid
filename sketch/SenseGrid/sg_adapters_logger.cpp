#include <Arduino.h>
#include "sg_adapters_logger.h"

extern "C" void sg_adapters_logger_init(void) {
  // usa API Arduino direta, nada de Print::printf aqui
  Serial.println("[logger] init");
}
