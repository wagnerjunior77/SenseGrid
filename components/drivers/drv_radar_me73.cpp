
#include "drv_radar_me73.h"
#include <string.h>

static inline uint16_t be16(const uint8_t* p) {
  return (uint16_t)((p[0] << 8) | p[1]);
}

static RadarRawFrame g_last_raw;
static bool g_has_last_raw = false;

static bool me73_begin(void* ctx, void* io) {
  return radar_begin(reinterpret_cast<RadarHandle*>(ctx),
                     reinterpret_cast<UartHandle*>(io));
}

static bool me73_read_raw(void* ctx, RadarRawFrame* out, uint32_t timeout_ms) {
  return radar_read_raw(reinterpret_cast<RadarHandle*>(ctx), out, timeout_ms);
}

static bool me73_read_parsed(void* ctx, RadarParsed* out, uint32_t timeout_ms) {
  return radar_read_parsed(reinterpret_cast<RadarHandle*>(ctx), out, timeout_ms);
}

static bool me73_get_last_raw(void* ctx, RadarRawFrame* out) {
  (void)ctx;
  return radar_get_last_raw(out);
}

const SgRadarOps SG_RADAR_ME73_OPS = {
  me73_begin,
  me73_read_raw,
  me73_read_parsed,
  me73_get_last_raw
};

bool radar_begin(RadarHandle* r, UartHandle* uart) {
  if (!r || !uart) return false;
  r->uart = uart;
  return true;
}

bool radar_read_raw(RadarHandle* r, RadarRawFrame* out, uint32_t timeout_ms) {
  if (!r || !r->uart || !out) return false;
  int n = uart_read_frame(r->uart, out->data, sizeof(out->data), timeout_ms, 0x55, 0xA5);
  if (n < 0) return false;
  out->size = (uint16_t)n;
  g_last_raw = *out;
  g_has_last_raw = true;
  return true;
}

bool radar_read_parsed(RadarHandle* r, RadarParsed* out, uint32_t timeout_ms) {
  if (!r || !out) return false;

  RadarRawFrame rf;
  if (!radar_read_raw(r, &rf, timeout_ms)) return false;
  if (rf.size < 8) return false; // hdr(2)+len(2)+min payload

  const uint8_t* buf = rf.data;
  const uint16_t len = (uint16_t)((buf[2] << 8) | buf[3]);
  const uint16_t total = (uint16_t)(4 + len);
  if (total != rf.size) return false;

  // Janela [FUNC..SUM]
  const uint8_t func = buf[4 + 0];
  const uint8_t cmd1 = buf[4 + 1];
  const uint8_t cmd2 = buf[4 + 2];
  const uint8_t* pay = &buf[4 + 3];
  const size_t pay_n = (len >= 4) ? (size_t)(len - 4) : 0; // sem o SUM
  const uint8_t expect_sum = buf[4 + len - 1];

  // --- checksum: aceita "legacy" e "full" ---
  auto sum_legacy = [&]() -> uint8_t {
    uint32_t s = func + cmd1 + cmd2;
    for (size_t i = 0; i < pay_n; ++i) s += pay[i];
    return (uint8_t)(s & 0xFF);
  };
  auto sum_full = [&]() -> uint8_t {
    uint32_t s = 0;
    // header(2) + len(2) + payload (exceto SUM)
    for (uint16_t i = 0; i < (uint16_t)(4 + len - 1); ++i) s += buf[i];
    return (uint8_t)(s & 0xFF);
  };
  const uint8_t c1 = sum_legacy();
  const uint8_t c2 = sum_full();
  if (expect_sum != c1 && expect_sum != c2) return false;

  // FUNC=0x03, CMD1=0x81 → telemetria de presença
  if (func == 0x03 && cmd1 == 0x81 && pay_n >= 10) {
    memset(out, 0, sizeof(*out));
    out->func        = func;
    out->cmd1        = cmd1;
    out->cmd2        = cmd2;
    out->target_id   = pay[0];
    out->status      = pay[1];          // enum do radar
    out->distance_cm = be16(&pay[2]);   // BE
    out->speed_cms   = (int16_t)be16(&pay[4]); // BE, signed
    out->azim_deg    = (int8_t)pay[6];
    out->elev_deg    = (int8_t)pay[7];
    out->signal      = be16(&pay[8]);   // BE (pode ser 8/10/12 bits, depende do fw)

    // normalização
    out->dist_m    = (float)out->distance_cm / 100.0f;
    out->speed_mps = (float)out->speed_cms / 100.0f;
    out->snr       = (RADAR_SIGNAL_MAX > 0.0f) ? (out->signal / RADAR_SIGNAL_MAX) : 0.0f;
    if (out->snr < 0.0f) out->snr = 0.0f;
    if (out->snr > 1.0f) out->snr = 1.0f;

    return true;
  }

  return false; // frame não reconhecido
}

bool radar_get_last_raw(RadarRawFrame* out) {
  if (!out || !g_has_last_raw) return false;
  *out = g_last_raw;
  return true;
}
