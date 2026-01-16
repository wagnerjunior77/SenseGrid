#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void sg_adapters_logger_init(void);
void sg_adapters_logger_set_enabled(int enable);
int sg_adapters_logger_enabled(void);
void sg_adapters_logger_set_device_id(const char* device_id, unsigned char contract_version);
void sg_adapters_logger_clear(void);
unsigned long sg_adapters_logger_size(void);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <Arduino.h>
#include "../../components/core/sg_core.h"
bool sg_adapters_logger_on_measurement(const SgCoreSnapshot* snap);
bool sg_adapters_logger_export(Print& out);
#endif
