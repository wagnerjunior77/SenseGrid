#include "sg_config.h"
#include <Preferences.h>
#include <string.h>

static const char* NAMESPACE_MQTT = "mqtt";

void sg_config_mqtt_load(SgMqttCfgStored* out, const SgMqttCfgStored* defaults) {
  if (!out) return;
  Preferences prefs;
  prefs.begin(NAMESPACE_MQTT, true);
  const char* def_host = defaults ? defaults->host : "";
  uint16_t def_port    = defaults ? defaults->port : 1883;
  String host = prefs.getString("host", def_host);
  uint32_t port = prefs.getUInt("port", def_port);
  prefs.end();
  memset(out, 0, sizeof(*out));
  host.toCharArray(out->host, sizeof(out->host));
  out->port = (uint16_t)port;
}

void sg_config_mqtt_save(const SgMqttCfgStored* cfg) {
  if (!cfg) return;
  Preferences prefs;
  prefs.begin(NAMESPACE_MQTT, false);
  prefs.putString("host", cfg->host);
  prefs.putUInt("port", cfg->port);
  prefs.end();
}
