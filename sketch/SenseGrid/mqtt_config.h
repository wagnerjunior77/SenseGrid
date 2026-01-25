#pragma once
#include <stdint.h>
#include "components/config/sg_config.h"

struct MqttRuntimeCfg {
  char     host[64];
  uint16_t port;
};

void mqtt_config_load(MqttRuntimeCfg* out, const MqttRuntimeCfg* defaults);
void mqtt_config_save(const MqttRuntimeCfg* cfg);
