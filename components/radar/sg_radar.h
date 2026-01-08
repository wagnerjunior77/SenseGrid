#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sg_radar_types.h"

// Driver ops table (sensor-agnostic).
typedef struct SgRadarOps {
  bool (*begin)(void* ctx, void* io);
  bool (*read_raw)(void* ctx, RadarRawFrame* out, uint32_t timeout_ms);
  bool (*read_parsed)(void* ctx, RadarParsed* out, uint32_t timeout_ms);
  bool (*get_last_raw)(void* ctx, RadarRawFrame* out);
} SgRadarOps;

// Generic radar instance (ops + context).
typedef struct SgRadar {
  const SgRadarOps* ops;
  void*             ctx;
} SgRadar;

static inline bool sg_radar_begin(SgRadar* r, void* io) {
  if (!r || !r->ops || !r->ops->begin) return false;
  return r->ops->begin(r->ctx, io);
}

static inline bool sg_radar_read_raw(SgRadar* r, RadarRawFrame* out, uint32_t timeout_ms) {
  if (!r || !r->ops || !r->ops->read_raw) return false;
  return r->ops->read_raw(r->ctx, out, timeout_ms);
}

static inline bool sg_radar_read_parsed(SgRadar* r, RadarParsed* out, uint32_t timeout_ms) {
  if (!r || !r->ops || !r->ops->read_parsed) return false;
  return r->ops->read_parsed(r->ctx, out, timeout_ms);
}

static inline bool sg_radar_get_last_raw(SgRadar* r, RadarRawFrame* out) {
  if (!r || !r->ops || !r->ops->get_last_raw) return false;
  return r->ops->get_last_raw(r->ctx, out);
}
