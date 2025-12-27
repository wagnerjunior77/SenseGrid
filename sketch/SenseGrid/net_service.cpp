#include "net_service.h"

void net_service_init(SgNetInfo* info) {
  sg_net_init(info);
  if (!info) return;
  Serial.printf("[NET] AP %s IP=%s\n",
                info->ssid_ap,
                info->ap_ip.toString().c_str());
  if (info->sta_connected) {
    Serial.printf("[NET] STA conectado em %s IP=%s\n",
                  info->ssid_sta,
                  info->sta_ip.toString().c_str());
  } else {
    Serial.println("[NET] STA nao conectou; mantendo apenas AP");
  }
  Serial.printf("[NET] MAC=%s\n", info->mac);
}

void net_service_refresh(SgNetInfo* info) {
  sg_net_get_info(info);
}
