#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "../radar/sg_radar_types.h"
#include "../pipeline/sg_pipe.h"
#include "../pipeline/sg_calib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint16_t default_range_cm; // preset de range (ex.: 200)
  bool     load_from_nvs;    // true para carregar params de NVS (default)
} SgCoreConfig;

typedef struct {
  bool        has_meas;
  RadarParsed meas;
  uint32_t    meas_ms;
  bool        in_range;
  SgPipeOut   pipe;
  bool        pipe_enabled;
  uint16_t    range_cm;
} SgCoreSnapshot;

// Init pipeline/core. Se cfg for null, usa defaults e carrega NVS.
void sg_core_init(const SgCoreConfig* cfg);

// Alimenta o core com uma leitura (tipicamente apos o parser). Retorna snapshot atualizado.
SgCoreSnapshot sg_core_step(const RadarParsed* meas, uint32_t now_ms);

// Snapshot/params atuais.
const SgCoreSnapshot* sg_core_get_snapshot();
const SgParams*       sg_core_get_params();
bool                  sg_core_params_from_nvs();

// Ajustes de parametros (opcao de persistir em NVS).
void sg_core_set_params(const SgParams* p, bool persist);
void sg_core_set_range_cm(uint16_t cm, bool persist);
uint16_t sg_core_get_range_cm();

void sg_core_set_pipe_enabled(bool on, bool persist);
bool sg_core_pipe_enabled();

void sg_core_reset_baseline();

// Calibracao (proxy para sg_calib).
bool sg_core_calib_start(uint32_t dur_ms);
bool sg_core_calib_abort();
void sg_core_calib_reset();
SgCalibState    sg_core_calib_state();
SgCalibMetrics  sg_core_calib_metrics(uint32_t now_ms);
SgCalibSuggest  sg_core_calib_suggest();
bool sg_core_calib_apply(bool persist);

#ifdef __cplusplus
}
#endif
