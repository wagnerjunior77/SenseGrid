#include <Arduino.h>
#include "../include/sg_adapters_logger.h"
#if __has_include("../../components/adapters_logger/include/adapters_logger.h")
extern "C" {
#include "../../components/adapters_logger/include/adapters_logger.h"
}
#endif
extern "C" void func(void);
extern "C" void sg_adapters_logger_init(void) { Serial.printf("[SenseGrid][adapters_logger] init\n"); func(); }
extern "C" void sg_adapters_logger_tick(void) { }
