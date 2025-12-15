#include "include/adapters_http.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

void sg_http_init(SgHttpCtx* ctx, const char* device_id, uint8_t contract_version) {
  sg_ser_init(ctx, device_id, contract_version);
}

uint32_t sg_http_next_seq(SgHttpCtx* ctx) {
  return sg_ser_next_seq(ctx);
}

size_t sg_http_envelope(const SgHttpCtx* ctx, uint32_t ts_ms, const char* type,
                        const char* payload_json, char* out, size_t out_sz) {
  return sg_ser_envelope(ctx, ts_ms, type, payload_json, out, out_sz);
}

size_t sg_http_make_occupancy(const SgHttpCtx* ctx, const SgHttpOccupancy* occ,
                              char* out, size_t out_sz) {
  return sg_ser_make_occupancy(ctx, occ, out, out_sz);
}

size_t sg_http_make_tracks(const SgHttpCtx* ctx, const SgHttpTracks* t,
                           char* out, size_t out_sz) {
  return sg_ser_make_tracks(ctx, t, out, out_sz);
}

size_t sg_http_make_health(const SgHttpCtx* ctx, const SgHttpHealth* h,
                           char* out, size_t out_sz) {
  return sg_ser_make_health(ctx, h, out, out_sz);
}

size_t sg_http_make_ack(const SgHttpCtx* ctx, uint32_t ts_ms, const char* txid,
                        int ok, char* out, size_t out_sz) {
  return sg_ser_make_ack(ctx, ts_ms, txid, ok, out, out_sz);
}

size_t sg_http_make_err(const SgHttpCtx* ctx, uint32_t ts_ms, const char* txid,
                        const char* code, const char* msg,
                        char* out, size_t out_sz) {
  return sg_ser_make_err(ctx, ts_ms, txid, code, msg, out, out_sz);
}
