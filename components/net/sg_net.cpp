#include "sg_net.h"

// Credenciais padrao (mantem comportamento atual)
static const char* STA_SSID = "PIZZIOLO_2G";
static const char* STA_PASS = "revil2301revil2301";
static const char* AP_SSID  = "SenseGrid";
static const char* AP_PASS  = "esp929305";

static void fill_info(SgNetInfo* info) {
  if (!info) return;
  info->sta_connected = (WiFi.status() == WL_CONNECTED);
  info->sta_ip = WiFi.localIP();
  info->ap_ip  = WiFi.softAPIP();
  String ssta = WiFi.SSID();
  String sap  = WiFi.softAPSSID();
  ssta.toCharArray(info->ssid_sta, sizeof(info->ssid_sta));
  sap.toCharArray(info->ssid_ap, sizeof(info->ssid_ap));
  uint8_t mac[6]; WiFi.macAddress(mac);
  snprintf(info->mac, sizeof(info->mac),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void sg_net_init(SgNetInfo* info) {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);

  WiFi.begin(STA_SSID, STA_PASS);
  uint32_t t_sta = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t_sta) < 10000) {
    delay(200);
  }

  fill_info(info);
}
