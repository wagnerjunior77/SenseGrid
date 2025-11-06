// components/drivers/drv_radar_me73.h
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../hal/hal_uart.h"

struct RadarRawFrame {
  uint8_t  data[256];
  uint16_t size;
};

struct RadarParsed {
  uint8_t  func;
  uint8_t  cmd1;
  uint8_t  cmd2;
  uint8_t  target_id;
  uint8_t  status;
  uint16_t distance_cm;
  int16_t  speed_cms;
  int8_t   pitch_deg;
  uint16_t signal;
};

struct RadarHandle {
  UartHandle* uart;
};

bool radar_begin(RadarHandle* r, UartHandle* uart);
bool radar_read_raw(RadarHandle* r, RadarRawFrame* out, uint32_t timeout_ms);
bool radar_read_parsed(RadarHandle* r, RadarParsed* out, uint32_t timeout_ms);
