#include <Arduino.h>
#include "../include/sg_core.h"
#if __has_include("../../components/core/include/core.h")
extern "C" {
#include "../../components/core/include/core.h"
}
#endif
extern "C" void func(void);
extern "C" void sg_core_init(void) { Serial.printf("[SenseGrid][core] init\n"); func(); }
extern "C" void sg_core_tick(void) { }
