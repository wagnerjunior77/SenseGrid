#include "sg_pipe.h"

static SgParams P;
static SgState   stable_state;
static uint32_t  stable_since_ms;
static SgState   last_seen_state;
static uint32_t  last_seen_change_ms;
static float     baseline_snr;
static bool      baseline_init = false;
static bool      inited = false;

void sg_pipe_init() {
  P.max_range_cm   = 200;     // 2,00 m (bate com o que setamos no radar)
  P.hold_empty_ms  = 600;     // segura um pouco p/ “sumir” presença
  P.hold_exist_ms  = 250;     // confirma presença estática rápido
  P.hold_motion_ms = 150;     // movimento estabiliza mais rápido
  P.snr_on_exist   = 0.10f;   // precisa SNR >= 0.10 p/ PRESENCE
  P.snr_off_exist  = 0.05f;   // cai p/ EMPTY quando SNR < 0.05
  // Novos parâmetros de A3 (baseline + gates adicionais)
  P.snr_min        = 0.05f;   // gate duro
  P.snr_move       = 0.15f;   // MOTION só se SNR >= 0.15
  P.delta_exist    = 0.05f;   // presença se acima do baseline + delta
  P.speed_thr_cms  = 5;       // |speed| mínimo para MOTION (cm/s)
  P.k_ema          = 0.02f;   // baseline EMA
  stable_state = SG_EMPTY;
  stable_since_ms = 0;
  last_seen_state = SG_EMPTY;
  last_seen_change_ms = 0;
  baseline_snr = 0.0f;
  baseline_init = false;
  inited = true;
}

void sg_pipe_set_params(const SgParams& p) { P = p; }

SgParams sg_pipe_get_params() { return P; }

void sg_pipe_reset_baseline() { baseline_init = false; baseline_snr = 0.0f; }

static SgState map_raw(uint8_t raw) {
  switch(raw) {
    case 1: return SG_MOTION;
    case 2: return SG_PRESENCE;
    default: return SG_EMPTY;
  }
}

SgPipeOut sg_pipe_step(const SgPipeIn& in) {
  if (!inited) sg_pipe_init();

  bool gated = (in.dist_cm > 0 && in.dist_cm <= P.max_range_cm);
  SgState cand = gated ? map_raw(in.raw_status) : SG_EMPTY;

  float snr = in.snr;
  if (snr < 0.0f) snr = 0.0f;
  if (snr > 1.0f) snr = 1.0f;

  if (!baseline_init) {
    baseline_snr = snr;
    baseline_init = true;
  } else {
    baseline_snr = baseline_snr * (1.0f - P.k_ema) + snr * P.k_ema;
    if (baseline_snr < 0.0f) baseline_snr = 0.0f;
    if (baseline_snr > 1.0f) baseline_snr = 1.0f;
  }

  uint32_t now = in.now_ms;

  // Gate duro: se fora do range, zeramos imediatamente o estado
  if (!gated) {
    stable_state = SG_EMPTY;
    stable_since_ms = now;
    last_seen_state = SG_EMPTY;
    last_seen_change_ms = now;
    SgPipeOut out;
    out.state     = SG_EMPTY;
    out.stable    = SG_EMPTY;
    out.stable_ms = 0;
    out.gated     = false;
    return out;
  }

  // Gate duro de SNR
  if (snr < P.snr_min) {
    cand = SG_EMPTY;
  }

  // MOTION precisa SNR e velocidade
  if (cand == SG_MOTION) {
    uint16_t abs_speed = (uint16_t)((in.speed_cms < 0) ? -in.speed_cms : in.speed_cms);
    if (snr < P.snr_move || abs_speed < P.speed_thr_cms) {
      cand = SG_PRESENCE;
    }
  }

  // Presença: precisa estar acima do baseline + delta
  if (cand == SG_PRESENCE) {
    float threshold = baseline_snr + P.delta_exist;
    if (snr < threshold) {
      cand = SG_EMPTY;
    }
  }

  // Histerese por SNR para presença estática
  if (cand == SG_PRESENCE) {
    if (in.snr < P.snr_on_exist) cand = SG_EMPTY;
  } else if (stable_state == SG_PRESENCE) {
    // se já está estável em PRESENCE, só cai quando SNR < off
    if (in.snr >= P.snr_off_exist) {
      cand = SG_PRESENCE; // mantém enquanto não abaixo do off
    }
  }

  if (cand != last_seen_state) {
    last_seen_state = cand;
    last_seen_change_ms = now;
  }

  uint16_t dwell =
    (cand == SG_EMPTY   ? P.hold_empty_ms :
     cand == SG_MOTION  ? P.hold_motion_ms :
                          P.hold_exist_ms);

  if (cand != stable_state) {
    if (now - last_seen_change_ms >= dwell) {
      stable_state = cand;
      stable_since_ms = now;
    }
  }

  SgPipeOut out;
  out.state     = cand;
  out.stable    = stable_state;
  out.stable_ms = now - stable_since_ms;
  out.gated     = gated;
  return out;
}
