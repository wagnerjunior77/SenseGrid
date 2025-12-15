#include "sg_serializer.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static size_t safe_snprintf(char* out, size_t out_sz, const char* fmt, ...) {
  if (!out || out_sz == 0) return 0;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(out, out_sz, fmt, ap);
  va_end(ap);
  if (n < 0) return 0;
  if ((size_t)n >= out_sz) return out_sz - 1;
  return (size_t)n;
}

void sg_ser_init(SgSerCtx* ctx, const char* device_id, uint8_t contract_version) {
  if (!ctx) return;
  ctx->device_id = device_id;
  ctx->seq = 0;
  ctx->v = contract_version;
}

uint32_t sg_ser_next_seq(SgSerCtx* ctx) {
  if (!ctx) return 0;
  ctx->seq += 1;
  return ctx->seq;
}

size_t sg_ser_ts_iso(uint32_t ts_ms, char* out, size_t out_sz) {
  if (!out || out_sz == 0) return 0;
  uint32_t total_sec = ts_ms / 1000;
  uint32_t ms        = ts_ms % 1000;
  uint32_t sec       = total_sec % 60;
  uint32_t min       = (total_sec / 60) % 60;
  uint32_t hour      = (total_sec / 3600) % 24; // uptime modulo dia
  return safe_snprintf(out, out_sz, "1970-01-01T%02u:%02u:%02u.%03uZ",
                       (unsigned)hour, (unsigned)min, (unsigned)sec, (unsigned)ms);
}

size_t sg_ser_envelope(const SgSerCtx* ctx, uint32_t ts_ms, const char* type,
                       const char* payload_json, char* out, size_t out_sz) {
  if (!ctx || !type || !payload_json) return 0;
  char iso[32];
  sg_ser_ts_iso(ts_ms, iso, sizeof(iso));
  return safe_snprintf(out, out_sz,
    "{\"v\":%u,\"ts_ms\":%lu,\"ts_iso\":\"%s\",\"device_id\":\"%s\",\"seq\":%lu,\"type\":\"%s\",\"payload\":%s}",
    (unsigned)ctx->v,
    (unsigned long)ts_ms,
    iso,
    ctx->device_id ? ctx->device_id : "",
    (unsigned long)ctx->seq,
    type,
    payload_json);
}

size_t sg_ser_make_occupancy(const SgSerCtx* ctx, const SgSerOccupancy* occ,
                             char* out, size_t out_sz) {
  if (!ctx || !occ) return 0;
  char payload[128];
  safe_snprintf(payload, sizeof(payload),
    "{\"count\":%d,\"confidence\":%.3f}",
    occ->state,
    occ->confidence);
  return sg_ser_envelope(ctx, occ->ts_ms, "occupancy", payload, out, out_sz);
}

size_t sg_ser_make_tracks(const SgSerCtx* ctx, const SgSerTracks* t,
                          char* out, size_t out_sz) {
  if (!ctx || !t) return 0;
  char payload[64];
  safe_snprintf(payload, sizeof(payload),
    "{\"active\":%d}",
    t->count_active);
  return sg_ser_envelope(ctx, t->ts_ms, "tracks", payload, out, out_sz);
}

size_t sg_ser_make_health(const SgSerCtx* ctx, const SgSerHealth* h,
                          char* out, size_t out_sz) {
  if (!ctx || !h) return 0;
  char payload[128];
  safe_snprintf(payload, sizeof(payload),
    "{\"fw\":\"%s\",\"uptime_s\":%u,\"rssi_dbm\":%d}",
    "1.0.0",
    (unsigned)h->uptime_s,
    h->rssi_dbm);
  return sg_ser_envelope(ctx, h->ts_ms, "health", payload, out, out_sz);
}

size_t sg_ser_make_ack(const SgSerCtx* ctx, uint32_t ts_ms, const char* txid,
                       int ok, char* out, size_t out_sz) {
  if (!ctx) return 0;
  char payload[128];
  safe_snprintf(payload, sizeof(payload),
    "{\"txid\":\"%s\",\"ok\":%s}",
    txid ? txid : "",
    ok ? "true" : "false");
  return sg_ser_envelope(ctx, ts_ms, "ack", payload, out, out_sz);
}

size_t sg_ser_make_err(const SgSerCtx* ctx, uint32_t ts_ms, const char* txid,
                       const char* code, const char* msg,
                       char* out, size_t out_sz) {
  if (!ctx) return 0;
  char payload[192];
  safe_snprintf(payload, sizeof(payload),
    "{\"txid\":\"%s\",\"code\":\"%s\",\"msg\":\"%s\"}",
    txid ? txid : "",
    code ? code : "err",
    msg ? msg : "");
  return sg_ser_envelope(ctx, ts_ms, "err", payload, out, out_sz);
}

const char* sg_ser_topic(SgSerTopic t) {
  switch (t) {
    case SG_SER_TOPIC_MEAS:       return "meas";
    case SG_SER_TOPIC_MEAS_RAW:   return "meas_raw";
    case SG_SER_TOPIC_EVENTS:     return "events";
    case SG_SER_TOPIC_STATUS:     return "status";
    case SG_SER_TOPIC_CAP:        return "cap";
    case SG_SER_TOPIC_ACK:        return "ack";
    case SG_SER_TOPIC_ERR:        return "err";
    case SG_SER_TOPIC_DT_META:    return "dt/meta";
    case SG_SER_TOPIC_DT_ST:      return "dt/st";
    case SG_SER_TOPIC_DT_OTA:     return "dt/ota";
    case SG_SER_TOPIC_DT_CFG_OUT: return "dt/cfg/out/output_1";
    case SG_SER_TOPIC_DT_CFG_IN:  return "dt/cfg/in/input_1";
    case SG_SER_TOPIC_DT_O_OUT:   return "dt/o/out/output_1";
    case SG_SER_TOPIC_DT_O_IN:    return "dt/o/in/input_1";
    default:                      return "";
  }
}

const char* sg_ser_path(SgSerPath p) {
  switch (p) {
    case SG_SER_PATH_OCCUPANCY: return "/v1/occupancy";
    case SG_SER_PATH_TRACKS:    return "/v1/tracks";
    case SG_SER_PATH_HEALTH:    return "/v1/health";
    case SG_SER_PATH_MEAS:      return "/v1/meas";
    case SG_SER_PATH_CMD:       return "/v1/cmd";
    default:                    return "";
  }
}
