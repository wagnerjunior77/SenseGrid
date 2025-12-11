#pragma once
#include <stdint.h>
#include <stddef.h>
#include <Arduino.h>
#include <WiFiClient.h>

typedef void (*SgMqttCmdCb)(const char* payload, size_t len);

struct SgMqttCfg {
  const char* host;
  uint16_t    port;
  const char* user;
  const char* pass;
  const char* sb_ref;    // smartbuilding_reference
  const char* device_id;
  uint16_t    keepalive_s;
};

void sg_mqtt_init(const SgMqttCfg* cfg, SgMqttCmdCb cmd_cb);
void sg_mqtt_loop();
bool sg_mqtt_connected();

// Publish helpers (QoS 0 ou 1, retain opcional)
bool sg_mqtt_publish(const char* topic_suf, const char* payload, bool retain, uint8_t qos);

// Conveniencias por tipo
bool sg_mqtt_pub_meas(const char* payload);
bool sg_mqtt_pub_event(const char* payload);
bool sg_mqtt_pub_status(const char* payload);
bool sg_mqtt_pub_cap(const char* payload);
bool sg_mqtt_pub_ack(const char* payload);
bool sg_mqtt_pub_err(const char* payload);
