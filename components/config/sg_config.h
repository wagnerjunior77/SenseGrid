#pragma once
#include <stdint.h>

// Config persistente de rede (STA/AP)
typedef struct {
  char sta_ssid[33];
  char sta_pass[65];
  char ap_ssid[33];
  char ap_pass[65];
} SgNetCfgStored;

// Config persistente de MQTT (host/porta)
typedef struct {
  char     host[64];
  uint16_t port;
} SgMqttCfgStored;

// Carrega de NVS; se nao existir, usa defaults fornecidos.
void sg_config_net_load(SgNetCfgStored* out, const SgNetCfgStored* defaults);
void sg_config_mqtt_load(SgMqttCfgStored* out, const SgMqttCfgStored* defaults);
// Salva em NVS.
void sg_config_net_save(const SgNetCfgStored* cfg);
void sg_config_mqtt_save(const SgMqttCfgStored* cfg);
