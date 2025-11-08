
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../hal/hal_uart.h"

#ifndef RADAR_SIGNAL_MAX
// Ajuste conforme seu firmware: 255 ou 1023/4095.
#define RADAR_SIGNAL_MAX 255.0f
#endif

struct RadarRawFrame {
  uint8_t  data[256];
  uint16_t size;
};

struct RadarParsed {
  // metadados do frame
  uint8_t  func;
  uint8_t  cmd1;
  uint8_t  cmd2;
  uint8_t  target_id;

  // brutos
  uint8_t  status;        // 0 none, 1 move, 2 exist (fornecido pelo radar)
  uint16_t distance_cm;   // cm (BE no firmware observado)
  int16_t  speed_cms;     // cm/s (BE, signed)
  int8_t   pitch_deg;     // opcional (se aplicável)
  uint16_t signal;        // nível relativo (BE, 0..255/1023/4095)

  // normalizados (SI)
  float    dist_m;        // m
  float    speed_mps;     // m/s
  float    snr;           // 0..1
};

struct RadarHandle {
  UartHandle* uart;
};

bool radar_begin(RadarHandle* r, UartHandle* uart);
bool radar_read_raw(RadarHandle* r, RadarRawFrame* out, uint32_t timeout_ms);
bool radar_read_parsed(RadarHandle* r, RadarParsed* out, uint32_t timeout_ms);
