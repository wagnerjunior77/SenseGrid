#pragma once
#include <stdint.h>
#include "../../components/core/sg_core.h"
#include "telemetry_ctx.h"

struct SgNetInfo;

void http_service_init(SgTelemetryCtx* tctx, SgNetInfo* net_info);
void http_service_loop(const SgCoreSnapshot* snap);
