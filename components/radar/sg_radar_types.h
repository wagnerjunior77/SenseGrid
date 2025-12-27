#pragma once
#include <stdint.h>

// Common radar data types shared across drivers/core.
typedef struct RadarRawFrame {
  uint8_t  data[256];
  uint16_t size;
} RadarRawFrame;

typedef struct RadarParsed {
  // Frame metadata
  uint8_t  func;
  uint8_t  cmd1;
  uint8_t  cmd2;
  uint8_t  target_id;

  // Raw fields
  uint8_t  status;        // 0 none, 1 move, 2 exist
  uint16_t distance_cm;   // cm (BE on observed firmware)
  int16_t  speed_cms;     // cm/s (BE, signed)
  int8_t   pitch_deg;     // optional
  uint16_t signal;        // relative level

  // Normalized (SI)
  float    dist_m;        // m
  float    speed_mps;     // m/s
  float    snr;           // 0..1
} RadarParsed;
