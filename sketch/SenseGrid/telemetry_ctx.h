#pragma once
#include <stdint.h>
#include "../../components/adapters_http/include/adapters_http.h"

struct SgTelemetryCtx {
  char     device_id[24];
  SgHttpCtx http;
};

void sg_telemetry_init(SgTelemetryCtx* ctx, uint8_t contract_version);
const char* sg_telemetry_device_id(const SgTelemetryCtx* ctx);
SgHttpCtx* sg_telemetry_http_ctx(SgTelemetryCtx* ctx);
