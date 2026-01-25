#include <Arduino.h>
#include <FS.h>
#include <SPIFFS.h>
#include <string.h>
#include "sg_adapters_logger.h"
#include "components/serializer/sg_serializer.h"

static bool g_inited = false;
static bool g_enabled = true;
static SgSerCtx g_ser{};
static bool g_ser_ready = false;
static int g_last_state = -1;
static uint32_t g_last_kpi_end = 0;
static uint32_t g_last_meas_log_ms = 0;

static const size_t LOG_MAX_BYTES = 256 * 1024;
static const int LOG_ROTATE_COUNT = 3;
static const uint32_t LOG_MEAS_PERIOD_MS = 200;

static void log_path(int idx, char* out, size_t out_sz) {
  if (!out || out_sz == 0) return;
  snprintf(out, out_sz, "/log%d.jsonl", idx);
}

static void log_rotate() {
  if (LOG_ROTATE_COUNT <= 1) return;
  char src[24];
  char dst[24];
  log_path(LOG_ROTATE_COUNT - 1, src, sizeof(src));
  if (SPIFFS.exists(src)) SPIFFS.remove(src);
  for (int i = LOG_ROTATE_COUNT - 2; i >= 0; --i) {
    log_path(i, src, sizeof(src));
    log_path(i + 1, dst, sizeof(dst));
    if (SPIFFS.exists(src)) {
      SPIFFS.rename(src, dst);
    }
  }
}

static bool log_append_line(const char* line) {
  if (!g_inited || !g_enabled || !line) return false;
  size_t line_len = strlen(line);
  char path[24];
  log_path(0, path, sizeof(path));
  File f = SPIFFS.open(path, FILE_APPEND);
  if (!f) {
    f = SPIFFS.open(path, FILE_WRITE);
  }
  if (!f) return false;
  size_t cur = f.size();
  if ((cur + line_len + 1) > LOG_MAX_BYTES) {
    f.close();
    log_rotate();
    f = SPIFFS.open(path, FILE_WRITE);
    if (!f) return false;
  }
  f.write((const uint8_t*)line, line_len);
  f.write('\n');
  f.close();
  return true;
}

static const char* state_str(int st) {
  switch (st) {
    case 0: return "empty";
    case 1: return "presence";
    case 2: return "motion";
    default: return "unknown";
  }
}

extern "C" void sg_adapters_logger_init(void) {
  if (g_inited) return;
  g_inited = SPIFFS.begin(true);
  g_enabled = g_inited;
  g_last_meas_log_ms = 0;
  g_last_state = -1;
  g_last_kpi_end = 0;
}

extern "C" void sg_adapters_logger_set_enabled(int enable) {
  g_enabled = (enable != 0);
}

extern "C" int sg_adapters_logger_enabled(void) {
  return g_enabled ? 1 : 0;
}

extern "C" void sg_adapters_logger_set_device_id(const char* device_id, unsigned char contract_version) {
  sg_ser_init(&g_ser, device_id ? device_id : "", contract_version);
  g_ser_ready = true;
}

extern "C" void sg_adapters_logger_clear(void) {
  char path[24];
  for (int i = 0; i < LOG_ROTATE_COUNT; ++i) {
    log_path(i, path, sizeof(path));
    if (SPIFFS.exists(path)) SPIFFS.remove(path);
  }
}

extern "C" unsigned long sg_adapters_logger_size(void) {
  if (!g_inited) return 0;
  unsigned long total = 0;
  char path[24];
  for (int i = LOG_ROTATE_COUNT - 1; i >= 0; --i) {
    log_path(i, path, sizeof(path));
    if (!SPIFFS.exists(path)) continue;
    File f = SPIFFS.open(path, FILE_READ);
    if (!f) continue;
    total += (unsigned long)f.size();
    f.close();
  }
  return total;
}

bool sg_adapters_logger_on_measurement(const SgCoreSnapshot* snap) {
  if (!g_ser_ready || !snap || !snap->has_meas) return false;
  uint32_t now_ms = snap->meas_ms;
  bool log_meas = (g_last_meas_log_ms == 0) || ((now_ms - g_last_meas_log_ms) >= LOG_MEAS_PERIOD_MS);
  if (log_meas) g_last_meas_log_ms = now_ms;
  char payload[256];
  if (log_meas) {
    int pn = snprintf(payload, sizeof(payload),
      "{\"ts_ms\":%lu,\"status\":\"%s\",\"dist_m\":%.3f,\"speed_mps\":%.3f,"
      "\"snr\":%.3f,\"distance_cm\":%u,\"speed_cms\":%d,\"signal\":%u,"
      "\"az_deg\":%d,\"el_deg\":%d,"
      "\"state\":%d,\"stable\":%d,\"stable_ms\":%lu,\"in_range\":%s}",
      (unsigned long)snap->meas_ms,
      (snap->meas.status == 0 ? "none" : snap->meas.status == 1 ? "move" : snap->meas.status == 2 ? "exist" : "?"),
      snap->meas.distance_cm * 0.01f,
      snap->meas.speed_cms * 0.01f,
      snap->meas.snr,
      snap->meas.distance_cm,
      (int)snap->meas.speed_cms,
      snap->meas.signal,
      (int)snap->meas.azim_deg,
      (int)snap->meas.elev_deg,
      (int)snap->pipe.state,
      (int)snap->pipe.stable,
      (unsigned long)snap->pipe.stable_ms,
      snap->in_range ? "true" : "false"
    );
    if (pn > 0) {
      char env[384];
      sg_ser_next_seq(&g_ser);
      sg_ser_envelope(&g_ser, snap->meas_ms, "meas", payload, env, sizeof(env));
      log_append_line(env);
    }
  }

  if (g_last_state < 0) g_last_state = snap->pipe.stable;
  if (snap->pipe.stable != g_last_state) {
    g_last_state = snap->pipe.stable;
    char ep[96];
    int en = snprintf(ep, sizeof(ep),
      "{\"class\":\"presence.changed\",\"state\":\"%s\"}",
      state_str(g_last_state));
    if (en > 0) {
      char ev[192];
      sg_ser_next_seq(&g_ser);
      sg_ser_envelope(&g_ser, snap->meas_ms, "event", ep, ev, sizeof(ev));
      log_append_line(ev);
    }
  }

  const SgCoreKpi* kpi = sg_core_kpi_last();
  if (kpi && kpi->window_end_ms > 0 && kpi->window_end_ms != g_last_kpi_end) {
    g_last_kpi_end = kpi->window_end_ms;
    char kp[320];
    int kn = snprintf(kp, sizeof(kp),
      "{\"window_ms\":%lu,\"meas_total\":%lu,\"meas_valid\":%lu,\"snr_avg\":%.3f,"
      "\"latency_avg_ms\":%lu,\"latency_max_ms\":%lu,"
      "\"state_ratio\":{\"empty\":%.3f,\"presence\":%.3f,\"motion\":%.3f},"
      "\"transition_count\":%lu,\"fp_proxy_ratio\":%.3f,\"fn_proxy_ratio\":%.3f}",
      (unsigned long)kpi->window_ms,
      (unsigned long)kpi->meas_total,
      (unsigned long)kpi->meas_valid,
      kpi->snr_avg,
      (unsigned long)kpi->latency_avg_ms,
      (unsigned long)kpi->latency_max_ms,
      kpi->state_ratio_empty,
      kpi->state_ratio_presence,
      kpi->state_ratio_motion,
      (unsigned long)kpi->transition_count,
      kpi->fp_proxy_ratio,
      kpi->fn_proxy_ratio
    );
    if (kn > 0) {
      char ke[448];
      sg_ser_next_seq(&g_ser);
      sg_ser_envelope(&g_ser, kpi->window_end_ms, "kpi", kp, ke, sizeof(ke));
      log_append_line(ke);
    }
  }
  return true;
}

bool sg_adapters_logger_export(Print& out) {
  if (!g_inited) return false;
  char path[24];
  for (int i = LOG_ROTATE_COUNT - 1; i >= 0; --i) {
    log_path(i, path, sizeof(path));
    if (!SPIFFS.exists(path)) continue;
    File f = SPIFFS.open(path, FILE_READ);
    if (!f) continue;
    while (f.available()) {
      char buf[256];
      size_t n = f.read((uint8_t*)buf, sizeof(buf));
      if (n == 0) break;
      out.write((const uint8_t*)buf, n);
    }
    f.close();
  }
  return true;
}
