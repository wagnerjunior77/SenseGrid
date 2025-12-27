#pragma once
#include <Arduino.h>
#include <WiFi.h>

struct SgNetInfo {
  bool      sta_connected;
  IPAddress sta_ip;
  IPAddress ap_ip;
  char      ssid_sta[33];
  char      ssid_ap[33];
  char      mac[18];
};

// Inicializa AP + STA com credenciais padrao.
// Preenche info (IP/AP, MAC, SSID) se fornecido.
void sg_net_init(SgNetInfo* info);
