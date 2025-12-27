#include "mqtt_config.h"
#include <string.h>

void mqtt_config_load(MqttRuntimeCfg* out, const MqttRuntimeCfg* defaults) {
  if (!out) return;
  SgMqttCfgStored def{};
  if (defaults) {
    strlcpy(def.host, defaults->host, sizeof(def.host));
    def.port = defaults->port;
  }
  SgMqttCfgStored tmp{};
  sg_config_mqtt_load(&tmp, defaults ? &def : nullptr);
  memset(out, 0, sizeof(*out));
  strlcpy(out->host, tmp.host, sizeof(out->host));
  out->port = tmp.port;
}

void mqtt_config_save(const MqttRuntimeCfg* cfg) {
  if (!cfg) return;
  SgMqttCfgStored tmp{};
  strlcpy(tmp.host, cfg->host, sizeof(tmp.host));
  tmp.port = cfg->port;
  sg_config_mqtt_save(&tmp);
}
