#include <Arduino.h>
#include "sg_core.h"

extern "C" void sg_core_init(void) {
  Serial.println("[core] init");
}
