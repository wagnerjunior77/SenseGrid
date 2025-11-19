#include "sg_pipe.h"

static SgParams P;
static SgState   stable_state;
static uint32_t  stable_since_ms;
static SgState   last_seen_state;
static uint32_t  last_seen_change_ms;
static bool      inited = false;

void sg_pipe_init() {
  P.max_range_cm   = 200;     // 2,00 m (bate com o que setamos no radar)
  P.hold_empty_ms  = 600;     // segura um pouco p/ “sumir” presença
  P.hold_exist_ms  = 250;     // confirma presença estática rápido
  P.hold_motion_ms = 150;     // movimento estabiliza mais rápido
  P.snr_on_exist   = 0.10f;   // precisa SNR >= 0.10 p/ PRESENCE
  P.snr_off_exist  = 0.05f;   // cai p/ EMPTY quando SNR < 0.05
  stable_state = SG_EMPTY;
  stable_since_ms = 0;
  last_seen_state = SG_EMPTY;
  last_seen_change_ms = 0;
  inited = true;
}

void sg_pipe_set_params(const SgParams& p) { P = p; }

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

  // Histerese por SNR para presença estática
  if (cand == SG_PRESENCE) {
    if (in.snr < P.snr_on_exist) cand = SG_EMPTY;
  } else if (stable_state == SG_PRESENCE) {
    // se já está estável em PRESENCE, só cai quando SNR < off
    if (in.snr >= P.snr_off_exist) {
      cand = SG_PRESENCE; // mantém enquanto não abaixo do off
    }
  }

  uint32_t now = in.now_ms;

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
