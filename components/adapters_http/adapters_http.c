#include "adapters_http.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

void sg_http_init(SgHttpCtx* ctx, const char* device_id, uint8_t contract_version) {
  if (!ctx) return;
  ctx->device_id = device_id;
  ctx->seq = 0;
  ctx->v = contract_version;
}

uint32_t sg_http_next_seq(SgHttpCtx* ctx) {
  if (!ctx) return 0;
  ctx->seq += 1;
  return ctx->seq;
}

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

size_t sg_http_envelope(const SgHttpCtx* ctx, uint32_t ts_ms, const char* type,
                        const char* payload_json, char* out, size_t out_sz) {
  if (!ctx || !type || !payload_json) return 0;
  return safe_snprintf(out, out_sz,
    "{\"v\":%u,\"ts_ms\":%lu,\"device_id\":\"%s\",\"seq\":%lu,\"type\":\"%s\",\"payload\":%s}",
    (unsigned)ctx->v,
    (unsigned long)ts_ms,
    ctx->device_id ? ctx->device_id : "",
    (unsigned long)ctx->seq,
    type,
    payload_json);
}

size_t sg_http_make_occupancy(const SgHttpCtx* ctx, const SgHttpOccupancy* occ,
                              char* out, size_t out_sz) {
  if (!ctx || !occ) return 0;
  char payload[128];
  safe_snprintf(payload, sizeof(payload),
    "{\"count\":%d,\"confidence\":%.3f}",
    occ->state,
    occ->confidence);
  return sg_http_envelope(ctx, occ->ts_ms, "occupancy", payload, out, out_sz);
}

size_t sg_http_make_tracks(const SgHttpCtx* ctx, const SgHttpTracks* t,
                           char* out, size_t out_sz) {
  if (!ctx || !t) return 0;
  char payload[64];
  safe_snprintf(payload, sizeof(payload),
    "{\"active\":%d}",
    t->count_active);
  return sg_http_envelope(ctx, t->ts_ms, "tracks", payload, out, out_sz);
}

size_t sg_http_make_health(const SgHttpCtx* ctx, const SgHttpHealth* h,
                           char* out, size_t out_sz) {
  if (!ctx || !h) return 0;
  char payload[128];
  safe_snprintf(payload, sizeof(payload),
    "{\"fw\":\"%s\",\"uptime_s\":%u,\"rssi_dbm\":%d}",
    "1.0.0",
    (unsigned)h->uptime_s,
    h->rssi_dbm);
  return sg_http_envelope(ctx, h->ts_ms, "health", payload, out, out_sz);
}

size_t sg_http_make_ack(const SgHttpCtx* ctx, uint32_t ts_ms, const char* txid,
                        int ok, char* out, size_t out_sz) {
  if (!ctx) return 0;
  char payload[128];
  safe_snprintf(payload, sizeof(payload),
    "{\"txid\":\"%s\",\"ok\":%s}",
    txid ? txid : "",
    ok ? "true" : "false");
  return sg_http_envelope(ctx, ts_ms, "ack", payload, out, out_sz);
}

size_t sg_http_make_err(const SgHttpCtx* ctx, uint32_t ts_ms, const char* txid,
                        const char* code, const char* msg,
                        char* out, size_t out_sz) {
  if (!ctx) return 0;
  char payload[192];
  safe_snprintf(payload, sizeof(payload),
    "{\"txid\":\"%s\",\"code\":\"%s\",\"msg\":\"%s\"}",
    txid ? txid : "",
    code ? code : "err",
    msg ? msg : "");
  return sg_http_envelope(ctx, ts_ms, "err", payload, out, out_sz);
}
