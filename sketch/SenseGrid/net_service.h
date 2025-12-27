#pragma once
#include <Arduino.h>
#include "../../../components/net/sg_net.h"

// Inicializa rede (AP+STA) e loga estado resumido.
void net_service_init(SgNetInfo* info);

// Atualiza estrutura com estado atual do WiFi (sem reconectar).
void net_service_refresh(SgNetInfo* info);
