#pragma once
#include <Arduino.h>
#include "../util/sg_log.h"

// Estado de runtime controlado pelo CLI
struct SgCliState {
  bool stream_on = false;         // se true, imprime leituras contínuas
  bool json_on   = true;          // formato: JSON (true) ou texto (false)
  uint16_t rate_ms = 0;           // mínimo intervalo entre prints (0 = sem throttle)
};

void sg_cli_init(Stream* io);
void sg_cli_poll();

// getters para o sketch consultar
bool     sg_cli_stream_on();
bool     sg_cli_json_on();
uint16_t sg_cli_rate_ms();
