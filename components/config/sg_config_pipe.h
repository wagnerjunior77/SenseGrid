#pragma once
#include <stdbool.h>
#include "../pipeline/sg_pipe.h"

#ifdef __cplusplus
extern "C" {
#endif

// Persistência dos parâmetros do pipeline (NVS "pipe").
// Usa as mesmas chaves que estavam no .ino (dist_max, hold_e/p/m, snr_*, delta_ex, spd_thr, k_ema).

// Carrega de NVS. params_io deve vir preenchido com defaults; enabled_io opcional (default false).
// has_nvs_out indica se havia config válida persistida (versão 1).
void sg_config_pipe_load(SgParams* params_io, bool* enabled_io, bool* has_nvs_out);

// Salva em NVS. Faz backup do valor anterior (chaves prefixo b_).
void sg_config_pipe_save(const SgParams* params, bool enabled);

// Restaura backup salvo (se existir). Retorna false se não houver backup.
bool sg_config_pipe_restore_backup(SgParams* params_io, bool* enabled_io);

#ifdef __cplusplus
}
#endif
