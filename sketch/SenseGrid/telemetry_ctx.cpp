#include "telemetry_ctx.h"
#include <Arduino.h>
#include <string.h>

static void make_device_id(char* out, size_t out_sz) {
  if (!out || out_sz == 0) return;
  uint64_t mac = ESP.getEfuseMac();
  snprintf(out, out_sz, "sg-%04X%08X",
           (uint16_t)(mac >> 32), (uint32_t)mac);
}

void sg_telemetry_init(SgTelemetryCtx* ctx, uint8_t contract_version) {
  if (!ctx) return;
  memset(ctx, 0, sizeof(*ctx));
  make_device_id(ctx->device_id, sizeof(ctx->device_id));
  sg_http_init(&ctx->http, ctx->device_id, contract_version);
}

const char* sg_telemetry_device_id(const SgTelemetryCtx* ctx) {
  return ctx ? ctx->device_id : "";
}

SgHttpCtx* sg_telemetry_http_ctx(SgTelemetryCtx* ctx) {
  return ctx ? &ctx->http : nullptr;
}
