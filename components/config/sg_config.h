#pragma once
#include <stdint.h>

// Config persistente de MQTT (Preferences)
typedef struct {
  char     host[64];
  uint16_t port;
} SgMqttCfgStored;

// Carrega de NVS; se nao existir, usa defaults fornecidos.
void sg_config_mqtt_load(SgMqttCfgStored* out, const SgMqttCfgStored* defaults);
// Salva em NVS.
void sg_config_mqtt_save(const SgMqttCfgStored* cfg);
