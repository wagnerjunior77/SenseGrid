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

// Inicializa AP + STA usando credenciais persistidas (ou defaults) e preenche info.
void sg_net_init(SgNetInfo* info);

// Atualiza info lendo o estado atual do WiFi (nao reconecta).
void sg_net_get_info(SgNetInfo* info);

// Atualiza credenciais persistentes. Nao reconecta automaticamente.
bool sg_net_set_sta_credentials(const char* ssid, const char* pass);
bool sg_net_set_ap_credentials(const char* ssid, const char* pass);

// Limpa credenciais persistentes.
bool sg_net_clear_sta_credentials();
bool sg_net_clear_ap_credentials();

// Indica se ha credenciais armazenadas.
bool sg_net_has_sta_credentials();
bool sg_net_has_ap_credentials();
