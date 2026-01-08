#include "sg_net.h"
#include <string.h>
#include "../util/sg_log.h"
#include "../config/sg_config.h"

// Defaults intentionally empty; require provisioning to enable WiFi.
static const char* DEF_STA_SSID = "";
static const char* DEF_STA_PASS = "";
static const char* DEF_AP_SSID  = "";
static const char* DEF_AP_PASS  = "";

static const uint32_t STA_TIMEOUT_MS = 10000;
static const uint8_t  STA_MAX_RETRY  = 2;

static SgNetInfo g_last_info;
static bool g_prov_ap = false;

static void copy_str(const char* src, char* dst, size_t dst_sz) {
  if (!dst || dst_sz == 0) return;
  if (!src) src = "";
  size_t n = dst_sz - 1;
  strncpy(dst, src, n);
  dst[n] = 0;
}

static void load_net_cfg(SgNetCfgStored& cfg) {
  SgNetCfgStored def{};
  copy_str(DEF_STA_SSID, def.sta_ssid, sizeof(def.sta_ssid));
  copy_str(DEF_STA_PASS, def.sta_pass, sizeof(def.sta_pass));
  copy_str(DEF_AP_SSID,  def.ap_ssid,  sizeof(def.ap_ssid));
  copy_str(DEF_AP_PASS,  def.ap_pass,  sizeof(def.ap_pass));
  sg_config_net_load(&cfg, &def);
  // Fallback to defaults if any mandatory field is missing.
  if (!cfg.ap_ssid[0])  copy_str(def.ap_ssid,  cfg.ap_ssid,  sizeof(cfg.ap_ssid));
  if (!cfg.sta_ssid[0]) copy_str(def.sta_ssid, cfg.sta_ssid, sizeof(cfg.sta_ssid));
}

static void fill_info(SgNetInfo* info) {
  if (!info) return;
  info->sta_connected = (WiFi.status() == WL_CONNECTED);
  info->sta_ip = WiFi.localIP();
  info->ap_ip  = WiFi.softAPIP();
  memset(info->ssid_sta, 0, sizeof(info->ssid_sta));
  memset(info->ssid_ap, 0, sizeof(info->ssid_ap));
  String ssta = WiFi.SSID();
  String sap  = WiFi.softAPSSID();
  ssta.toCharArray(info->ssid_sta, sizeof(info->ssid_sta));
  sap.toCharArray(info->ssid_ap, sizeof(info->ssid_ap));
  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(info->mac, sizeof(info->mac),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void start_ap(const SgNetCfgStored& cfg) {
  if (!cfg.ap_ssid[0]) return;
  if (cfg.ap_pass[0]) {
    WiFi.softAP(cfg.ap_ssid, cfg.ap_pass);
  } else {
    WiFi.softAP(cfg.ap_ssid);
  }
}

static void reset_wifi() {
  WiFi.disconnect(true);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_MODE_NULL);
  delay(200);
}

static void start_provision_ap() {
  char ssid[32];
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(ssid, sizeof(ssid), "SenseGrid-Setup-%02X%02X", mac[4], mac[5]);
  WiFi.softAP(ssid);
  g_prov_ap = true;
}

static void connect_sta(const SgNetCfgStored& cfg) {
  if (!cfg.sta_ssid[0]) return;
  for (uint8_t attempt = 1; attempt <= STA_MAX_RETRY; ++attempt) {
    SG_LOG(SG_INFO, "NET STA connect try %u", (unsigned)attempt);
    WiFi.begin(cfg.sta_ssid, cfg.sta_pass);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - t0) < STA_TIMEOUT_MS) {
      delay(200);
    }
    if (WiFi.status() == WL_CONNECTED) {
      return;
    }
    WiFi.disconnect();
    delay(200);
  }
}

void sg_net_init(SgNetInfo* info) {
  SgNetCfgStored cfg{};
  load_net_cfg(cfg);

  bool has_ap = (cfg.ap_ssid[0] != 0);
  bool has_sta = (cfg.sta_ssid[0] != 0);
  g_prov_ap = false;

  if (!has_ap) SG_LOG(SG_INFO, "NET AP not configured");
  if (!has_sta) SG_LOG(SG_INFO, "NET STA not configured");

  reset_wifi();
  WiFi.mode(WIFI_AP_STA);
  if (has_ap) start_ap(cfg);
  connect_sta(cfg);
  bool sta_connected = (WiFi.status() == WL_CONNECTED);
  if (!has_ap && (!has_sta || !sta_connected)) {
    start_provision_ap();
  }

  fill_info(&g_last_info);
  if (info) *info = g_last_info;

  if (g_prov_ap) {
    SG_LOG(SG_INFO, "NET AP provision up IP=%s", g_last_info.ap_ip.toString().c_str());
  } else if (has_ap) {
    SG_LOG(SG_INFO, "NET AP up IP=%s", g_last_info.ap_ip.toString().c_str());
  }
  if (has_sta) {
    if (sta_connected) {
      SG_LOG(SG_INFO, "NET STA connected IP=%s", g_last_info.sta_ip.toString().c_str());
    } else {
      SG_LOG(SG_WARN, "NET STA not connected; AP only");
    }
  }
  SG_LOG(SG_INFO, "NET MAC=%s", g_last_info.mac);
}

void sg_net_get_info(SgNetInfo* info) {
  fill_info(&g_last_info);
  if (info) *info = g_last_info;
}

bool sg_net_set_sta_credentials(const char* ssid, const char* pass) {
  if (!ssid || !ssid[0]) return false;
  SgNetCfgStored cfg{};
  load_net_cfg(cfg);
  copy_str(ssid, cfg.sta_ssid, sizeof(cfg.sta_ssid));
  copy_str(pass, cfg.sta_pass, sizeof(cfg.sta_pass));
  sg_config_net_save(&cfg);
  return true;
}

bool sg_net_set_ap_credentials(const char* ssid, const char* pass) {
  if (!ssid || !ssid[0]) return false;
  SgNetCfgStored cfg{};
  load_net_cfg(cfg);
  copy_str(ssid, cfg.ap_ssid, sizeof(cfg.ap_ssid));
  copy_str(pass, cfg.ap_pass, sizeof(cfg.ap_pass));
  sg_config_net_save(&cfg);
  return true;
}

bool sg_net_clear_sta_credentials() {
  SgNetCfgStored cfg{};
  sg_config_net_load(&cfg, nullptr);
  memset(cfg.sta_ssid, 0, sizeof(cfg.sta_ssid));
  memset(cfg.sta_pass, 0, sizeof(cfg.sta_pass));
  sg_config_net_save(&cfg);
  return true;
}

bool sg_net_clear_ap_credentials() {
  SgNetCfgStored cfg{};
  sg_config_net_load(&cfg, nullptr);
  memset(cfg.ap_ssid, 0, sizeof(cfg.ap_ssid));
  memset(cfg.ap_pass, 0, sizeof(cfg.ap_pass));
  sg_config_net_save(&cfg);
  return true;
}

bool sg_net_has_sta_credentials() {
  SgNetCfgStored cfg{};
  sg_config_net_load(&cfg, nullptr);
  return cfg.sta_ssid[0] != 0;
}

bool sg_net_has_ap_credentials() {
  SgNetCfgStored cfg{};
  sg_config_net_load(&cfg, nullptr);
  return cfg.ap_ssid[0] != 0;
}
