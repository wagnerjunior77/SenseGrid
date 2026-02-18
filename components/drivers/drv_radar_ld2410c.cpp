#include "drv_radar_ld2410c.h"
#include <string.h>
#include <Arduino.h>

static RadarRawFrame g_last_raw_ld;
static bool g_has_last_raw_ld = false;

static inline uint16_t le16(const uint8_t* p) {
  return (uint16_t)(p[0] | (uint16_t)(p[1] << 8));
}

static bool timed_read(HardwareSerial* ser, uint8_t& out, uint32_t& start_ms, uint32_t timeout_ms) {
  while ((millis() - start_ms) < timeout_ms) {
    if (ser->available() > 0) {
      int v = ser->read();
      if (v >= 0) { out = (uint8_t)v; return true; }
    }
    delay(1);
  }
  return false;
}

static bool read_report_frame(UartHandle* uart, RadarRawFrame* out, uint32_t timeout_ms) {
  if (!uart || !uart->impl || !out) return false;
  HardwareSerial* ser = reinterpret_cast<HardwareSerial*>(uart->impl);
  const uint8_t hdr[4]  = {0xF4, 0xF3, 0xF2, 0xF1};
  const uint8_t tail[4] = {0xF8, 0xF7, 0xF6, 0xF5};
  uint32_t t0 = millis();

  // find header sequence
  int hidx = 0;
  while ((millis() - t0) < timeout_ms) {
    if (ser->available() > 0) {
      int v = ser->read();
      if (v < 0) continue;
      uint8_t b = (uint8_t)v;
      if (b == hdr[hidx]) {
        hidx++;
        if (hidx == 4) break;
      } else {
        hidx = (b == hdr[0]) ? 1 : 0;
      }
    } else {
      delay(1);
    }
  }
  if (hidx != 4) return false;

  // length (little-endian)
  uint8_t len_lo = 0, len_hi = 0;
  if (!timed_read(ser, len_lo, t0, timeout_ms)) return false;
  if (!timed_read(ser, len_hi, t0, timeout_ms)) return false;
  uint16_t len = (uint16_t)(len_lo | (uint16_t)(len_hi << 8));

  size_t total = 4 + 2 + (size_t)len + 4;
  if (total > sizeof(out->data)) {
    // drain payload + tail
    for (uint16_t i = 0; i < len + 4; ++i) {
      uint8_t dump = 0;
      if (!timed_read(ser, dump, t0, timeout_ms)) break;
    }
    return false;
  }

  // store header and len
  size_t idx = 0;
  out->data[idx++] = hdr[0];
  out->data[idx++] = hdr[1];
  out->data[idx++] = hdr[2];
  out->data[idx++] = hdr[3];
  out->data[idx++] = len_lo;
  out->data[idx++] = len_hi;

  // read data
  for (uint16_t i = 0; i < len; ++i) {
    uint8_t d = 0;
    if (!timed_read(ser, d, t0, timeout_ms)) return false;
    out->data[idx++] = d;
  }

  // read and validate tail
  for (int i = 0; i < 4; ++i) {
    uint8_t d = 0;
    if (!timed_read(ser, d, t0, timeout_ms)) return false;
    if (d != tail[i]) return false;
    out->data[idx++] = d;
  }

  out->size = (uint16_t)idx;
  g_last_raw_ld = *out;
  g_has_last_raw_ld = true;
  return true;
}

static bool ld_read_parsed_impl(Ld2410cHandle* r, RadarParsed* out, uint32_t timeout_ms) {
  if (!r || !out) return false;
  RadarRawFrame rf;
  if (!ld2410c_read_raw(r, &rf, timeout_ms)) return false;

  if (rf.size < 4 + 2 + 13 + 4) return false; // header+len+min payload+tail
  const uint8_t* buf = rf.data;
  const uint16_t len = (uint16_t)(buf[4] | (uint16_t)(buf[5] << 8));
  if ((size_t)(4 + 2 + len + 4) != rf.size) return false;

  const uint8_t* data = &buf[6];
  if (len < 13) return false;

  const uint8_t data_type = data[0];
  const uint8_t head = data[1];
  if (head != 0xAA) return false;
  if (!(data_type == 0x01 || data_type == 0x02)) return false;

  const uint8_t tgt_state = data[2];
  const uint16_t motion_dist = le16(&data[3]);
  const uint8_t motion_energy = data[5];
  const uint16_t static_dist = le16(&data[6]);
  const uint8_t static_energy = data[8];
  const uint16_t detect_dist = le16(&data[9]);
  if (len < 2) return false;
  if (data[len - 2] != 0x55) return false;

  uint16_t dist_cm = detect_dist;
  if (dist_cm == 0) dist_cm = (motion_dist > 0) ? motion_dist : static_dist;

  uint8_t status = 0;
  if (tgt_state == 0x01) status = 1;
  else if (tgt_state == 0x02) status = 2;
  else if (tgt_state == 0x03) status = 1;
  else status = 0;

  uint8_t signal = (motion_energy > static_energy) ? motion_energy : static_energy;

  memset(out, 0, sizeof(*out));
  out->func        = data_type;
  out->cmd1        = 0;
  out->cmd2        = 0;
  out->target_id   = 0;
  out->status      = status;
  out->distance_cm = dist_cm;
  out->speed_cms   = (status == 1) ? 10 : 0; // no real speed on LD2410C
  out->azim_deg    = 0;
  out->elev_deg    = 0;
  out->signal      = signal;

  out->dist_m    = (float)out->distance_cm / 100.0f;
  out->speed_mps = (float)out->speed_cms / 100.0f;
  out->snr       = (signal / 255.0f);
  if (out->snr < 0.0f) out->snr = 0.0f;
  if (out->snr > 1.0f) out->snr = 1.0f;
  return true;
}

static bool ld_begin(void* ctx, void* io) {
  return ld2410c_begin(reinterpret_cast<Ld2410cHandle*>(ctx),
                       reinterpret_cast<UartHandle*>(io));
}

static bool ld_read_raw(void* ctx, RadarRawFrame* out, uint32_t timeout_ms) {
  return ld2410c_read_raw(reinterpret_cast<Ld2410cHandle*>(ctx), out, timeout_ms);
}

static bool ld_read_parsed(void* ctx, RadarParsed* out, uint32_t timeout_ms) {
  return ld2410c_read_parsed(reinterpret_cast<Ld2410cHandle*>(ctx), out, timeout_ms);
}

static bool ld_get_last_raw(void* ctx, RadarRawFrame* out) {
  (void)ctx;
  return ld2410c_get_last_raw(out);
}

const SgRadarOps SG_RADAR_LD2410C_OPS = {
  ld_begin,
  ld_read_raw,
  ld_read_parsed,
  ld_get_last_raw
};

bool ld2410c_begin(Ld2410cHandle* r, UartHandle* uart) {
  if (!r || !uart) return false;
  r->uart = uart;
  return true;
}

bool ld2410c_read_raw(Ld2410cHandle* r, RadarRawFrame* out, uint32_t timeout_ms) {
  if (!r || !r->uart || !out) return false;
  return read_report_frame(r->uart, out, timeout_ms);
}

bool ld2410c_read_parsed(Ld2410cHandle* r, RadarParsed* out, uint32_t timeout_ms) {
  return ld_read_parsed_impl(r, out, timeout_ms);
}

bool ld2410c_get_last_raw(RadarRawFrame* out) {
  if (!out || !g_has_last_raw_ld) return false;
  *out = g_last_raw_ld;
  return true;
}

static bool send_cmd(UartHandle* uart, uint16_t cmd, const uint8_t* payload, size_t payload_len) {
  if (!uart || !uart->impl) return false;
  const uint8_t hdr[4]  = {0xFD, 0xFC, 0xFB, 0xFA};
  const uint8_t end[4]  = {0x04, 0x03, 0x02, 0x01};
  const uint16_t len = (uint16_t)(2 + payload_len);
  const size_t total = 4 + 2 + len + 4;
  if (total > 64) return false;
  uint8_t frame[64];
  size_t idx = 0;
  frame[idx++] = hdr[0];
  frame[idx++] = hdr[1];
  frame[idx++] = hdr[2];
  frame[idx++] = hdr[3];
  frame[idx++] = (uint8_t)(len & 0xFF);
  frame[idx++] = (uint8_t)((len >> 8) & 0xFF);
  frame[idx++] = (uint8_t)(cmd & 0xFF);
  frame[idx++] = (uint8_t)((cmd >> 8) & 0xFF);
  for (size_t i = 0; i < payload_len; ++i) frame[idx++] = payload[i];
  frame[idx++] = end[0];
  frame[idx++] = end[1];
  frame[idx++] = end[2];
  frame[idx++] = end[3];
  return (uart_write(uart, frame, idx) == idx);
}

bool ld2410c_cmd_enable(UartHandle* uart) {
  uint8_t payload[2] = {0x01, 0x00};
  return send_cmd(uart, 0x00FF, payload, sizeof(payload));
}

bool ld2410c_cmd_end(UartHandle* uart) {
  return send_cmd(uart, 0x00FE, nullptr, 0);
}

bool ld2410c_cmd_set_max_gates(UartHandle* uart,
                               uint8_t gate_motion,
                               uint8_t gate_static,
                               uint16_t no_one_s) {
  uint8_t payload[18];
  size_t idx = 0;
  // param word 0x0000 (max motion gate)
  payload[idx++] = 0x00; payload[idx++] = 0x00;
  payload[idx++] = gate_motion; payload[idx++] = 0x00; payload[idx++] = 0x00; payload[idx++] = 0x00;
  // param word 0x0001 (max static gate)
  payload[idx++] = 0x01; payload[idx++] = 0x00;
  payload[idx++] = gate_static; payload[idx++] = 0x00; payload[idx++] = 0x00; payload[idx++] = 0x00;
  // param word 0x0002 (no-one duration)
  payload[idx++] = 0x02; payload[idx++] = 0x00;
  payload[idx++] = (uint8_t)(no_one_s & 0xFF);
  payload[idx++] = (uint8_t)((no_one_s >> 8) & 0xFF);
  payload[idx++] = 0x00;
  payload[idx++] = 0x00;
  return send_cmd(uart, 0x0060, payload, idx);
}
