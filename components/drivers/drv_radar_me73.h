
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../hal/hal_uart.h"
#include "../radar/sg_radar.h"

#ifndef RADAR_SIGNAL_MAX
// Observado sinal até ~1685; usar 2047.0f dá headroom e evita saturar.
#define RADAR_SIGNAL_MAX 2047.0f
#endif

typedef struct RadarHandle {
  UartHandle* uart;
} RadarHandle;

bool radar_begin(RadarHandle* r, UartHandle* uart);
bool radar_read_raw(RadarHandle* r, RadarRawFrame* out, uint32_t timeout_ms);
bool radar_read_parsed(RadarHandle* r, RadarParsed* out, uint32_t timeout_ms);
bool radar_get_last_raw(RadarRawFrame* out);

// Ops table for generic radar interface.
extern const SgRadarOps SG_RADAR_ME73_OPS;
