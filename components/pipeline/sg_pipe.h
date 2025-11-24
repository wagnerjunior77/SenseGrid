#pragma once
#include <stdint.h>

enum SgState { SG_EMPTY=0, SG_PRESENCE=1, SG_MOTION=2 };

struct SgParams {
  uint16_t max_range_cm;      // gate por distância
  uint16_t hold_empty_ms;     // tempo p/ consolidar EMPTY
  uint16_t hold_exist_ms;     // tempo p/ consolidar PRESENCE
  uint16_t hold_motion_ms;    // tempo p/ consolidar MOTION
  float    snr_on_exist;      // limiar p/ aceitar PRESENCE
  float    snr_off_exist;     // limiar p/ derrubar PRESENCE
  float    snr_min;           // gate mínimo de SNR
  float    snr_move;          // SNR mínimo para considerar MOTION
  float    delta_exist;       // offset acima do baseline para PRESENCE
  uint16_t speed_thr_cms;     // |speed| mínimo para MOTION (cm/s)
  float    k_ema;             // ganho do EMA do baseline de SNR
};

struct SgPipeIn  { uint8_t raw_status; uint16_t dist_cm; int16_t speed_cms; float snr; uint32_t now_ms; };
struct SgPipeOut { SgState state; SgState stable; uint32_t stable_ms; bool gated; };

void sg_pipe_init();
void sg_pipe_set_params(const SgParams& p);
SgParams sg_pipe_get_params();
void sg_pipe_reset_baseline();
SgPipeOut sg_pipe_step(const SgPipeIn& in);
