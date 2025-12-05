#pragma once
#include <stdint.h>
#include "sg_pipe.h"

enum SgCalibState { SG_CALIB_IDLE=0, SG_CALIB_COLLECT, SG_CALIB_READY, SG_CALIB_APPLIED };

struct SgCalibMetrics {
  uint32_t samples_total;
  uint32_t samples_valid;
  uint32_t elapsed_ms;
  uint32_t target_ms;
  float    progress;     // 0..1
  float    snr_mean;
  float    snr_std;
  uint16_t dist_p95_cm;
  float    valid_ratio;
  bool     ready;
};

struct SgCalibSuggest {
  uint16_t max_range_cm;
  float    snr_min;
  float    delta_exist;
  uint16_t hold_empty_ms;
  uint16_t hold_exist_ms;
  uint16_t hold_motion_ms;
  float    k_ema;
};

// Inicia coleta (tipicamente 60s). Retorna false se j� estava coletando.
bool sg_calib_start(uint32_t dur_ms);

// Alimenta o coletor com uma amostra (tipicamente logo ap�s o parser).
void sg_calib_push_sample(const SgPipeIn& in);

// Aborta a coleta em andamento e volta para IDLE.
bool sg_calib_abort();

// Reseta o estado (descarta coleta/sugest�es).
void sg_calib_reset();

// Estado atual.
SgCalibState sg_calib_state();

// M�tricas parciais/definitivas (usa now_ms para elapsed).
SgCalibMetrics sg_calib_metrics(uint32_t now_ms);

// Constr�i sugest�es com base nas m�tricas e nos par�metros atuais.
SgCalibSuggest sg_calib_build_suggest(const SgParams& base);

// Aplica as sugest�es sobre o struct informado (in-place). Retorna false se n�o estiver pronto.
bool sg_calib_apply(SgParams* params_io);
