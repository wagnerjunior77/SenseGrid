#include "sg_config.h"
#include <Preferences.h>
#include <string.h>

static const char* NAMESPACE_NET  = "net";
static const char* NAMESPACE_MQTT = "mqtt";

void sg_config_net_load(SgNetCfgStored* out, const SgNetCfgStored* defaults) {
  if (!out) return;
  Preferences prefs;
  prefs.begin(NAMESPACE_NET, true);
  const char* def_sta_ssid = defaults ? defaults->sta_ssid : "";
  const char* def_sta_pass = defaults ? defaults->sta_pass : "";
  const char* def_ap_ssid  = defaults ? defaults->ap_ssid : "";
  const char* def_ap_pass  = defaults ? defaults->ap_pass : "";
  String sta_ssid = prefs.getString("sta_ssid", def_sta_ssid);
  String sta_pass = prefs.getString("sta_pass", def_sta_pass);
  String ap_ssid  = prefs.getString("ap_ssid",  def_ap_ssid);
  String ap_pass  = prefs.getString("ap_pass",  def_ap_pass);
  prefs.end();
  memset(out, 0, sizeof(*out));
  sta_ssid.toCharArray(out->sta_ssid, sizeof(out->sta_ssid));
  sta_pass.toCharArray(out->sta_pass, sizeof(out->sta_pass));
  ap_ssid.toCharArray(out->ap_ssid, sizeof(out->ap_ssid));
  ap_pass.toCharArray(out->ap_pass, sizeof(out->ap_pass));
}

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

void sg_config_net_save(const SgNetCfgStored* cfg) {
  if (!cfg) return;
  Preferences prefs;
  prefs.begin(NAMESPACE_NET, false);
  prefs.putString("sta_ssid", cfg->sta_ssid);
  prefs.putString("sta_pass", cfg->sta_pass);
  prefs.putString("ap_ssid",  cfg->ap_ssid);
  prefs.putString("ap_pass",  cfg->ap_pass);
  prefs.end();
}

void sg_config_mqtt_save(const SgMqttCfgStored* cfg) {
  if (!cfg) return;
  Preferences prefs;
  prefs.begin(NAMESPACE_MQTT, false);
  prefs.putString("host", cfg->host);
  prefs.putUInt("port", cfg->port);
  prefs.end();
}
