
#pragma once
#include <Arduino.h>
#include "../drivers/drv_radar_me73.h"

#ifdef __cplusplus
extern "C" {
#endif

// Imprime uma única linha JSON com os campos SI e brutos (útil para diagnóstico)
void radar_print_json(const RadarParsed* p, Stream& out, uint32_t ts_ms);

#ifdef __cplusplus
}
#endif
