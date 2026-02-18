#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../hal/hal_uart.h"
#include "../radar/sg_radar.h"

// LD2410C driver handle
typedef struct Ld2410cHandle {
  UartHandle* uart;
} Ld2410cHandle;

bool ld2410c_begin(Ld2410cHandle* r, UartHandle* uart);
bool ld2410c_read_raw(Ld2410cHandle* r, RadarRawFrame* out, uint32_t timeout_ms);
bool ld2410c_read_parsed(Ld2410cHandle* r, RadarParsed* out, uint32_t timeout_ms);
bool ld2410c_get_last_raw(RadarRawFrame* out);

// Minimal command helpers (no ACK parsing)
bool ld2410c_cmd_enable(UartHandle* uart);
bool ld2410c_cmd_end(UartHandle* uart);
bool ld2410c_cmd_set_max_gates(UartHandle* uart,
                               uint8_t gate_motion,
                               uint8_t gate_static,
                               uint16_t no_one_s);

// Ops table for generic radar interface.
extern const SgRadarOps SG_RADAR_LD2410C_OPS;
