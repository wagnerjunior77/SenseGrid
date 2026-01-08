#pragma once
#include <Arduino.h>
#include "../../../components/net/sg_net.h"

// Inicializa rede (AP+STA) e loga estado resumido.
void net_service_init(SgNetInfo* info);

// Atualiza estrutura com estado atual do WiFi (sem reconectar).
void net_service_refresh(SgNetInfo* info);

// Reaplica configuracao persistida (reinicia WiFi).
void net_service_apply();

// CLI: wifi show|set|ap|clear|apply
void net_service_cli(int argc, char* argv[], Print& out);

// Indica se ha credenciais armazenadas.
bool net_service_has_sta_credentials();
bool net_service_has_ap_credentials();
