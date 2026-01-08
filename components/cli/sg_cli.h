#pragma once
#include <Arduino.h>  // para Stream/Print

// Callbacks que o .ino registra
using SgCliHelpFn   = void(*)(Print& out);
using SgCliInfoFn   = void(*)(Print& out);
using SgCliStreamFn = void(*)(bool on);
using SgCliRateFn   = void(*)(uint32_t ms);
using SgCliJsonFn   = void(*)(bool on);
using SgCliLogFn    = void(*)(int level);
using SgCliPipeFn   = void(*)(int argc, char* argv[], Print& out);
using SgCliRangeFn  = void(*)(uint32_t cm);
using SgCliCalibFn  = void(*)(int argc, char* argv[], Print& out);
using SgCliRawFn    = void(*)(int argc, char* argv[], Print& out);
using SgCliMqttFn   = void(*)(int argc, char* argv[], Print& out);
using SgCliWifiFn   = void(*)(int argc, char* argv[], Print& out);

// Registra os handlers (help, info, stream, rate, json, log, pipe, range, calib, raw, mqtt, wifi)
void sg_cli_set_handlers(SgCliHelpFn h, SgCliInfoFn i, SgCliStreamFn s,
                         SgCliRateFn r, SgCliJsonFn j, SgCliLogFn l,
                         SgCliPipeFn p, SgCliRangeFn rg, SgCliCalibFn cb,
                         SgCliRawFn raw, SgCliMqttFn m, SgCliWifiFn w);

// Faz o parse linha-a-linha do que chega pela serial
void sg_cli_poll(Stream& in, Print& out);

// Versão prática: usa a mesma serial pra input e output
inline void sg_cli_poll(Stream& io) { sg_cli_poll(io, io); }

// Opcional: imprime o help padrão se você quiser chamar direto
void sg_cli_print_default_help(Print& out);
