#pragma once
#include <Arduino.h>
#include "components/core/sg_core.h"
#include "telemetry_ctx.h"

typedef void (*MqttApplyRangeFn)(uint16_t cm);

void mqtt_service_init(SgTelemetryCtx* tctx, MqttApplyRangeFn apply_range_cb);
void mqtt_service_loop();
void mqtt_service_on_measurement(const SgCoreSnapshot* snap);
void mqtt_service_cli(int argc, char* argv[], Print& out);
