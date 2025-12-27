#include "sg_core.h"
#include "../config/sg_config_pipe.h"

// Estado interno
static SgParams       g_params;
static bool           g_pipe_enabled = false;
static bool           g_params_from_nvs = false;
static SgCoreSnapshot g_snap;

// Helpers
static uint16_t clamp_range_cm(uint32_t cm) {
  if (cm > 0 && cm <= 10) cm *= 100;
  if (cm <= 250) return 200;
  if (cm <= 500) return 400;
  return 600;
}

static SgPipeOut passthrough(const SgPipeIn& in, bool in_range) {
  SgState pass = in_range
                  ? (in.raw_status == 1 ? SG_MOTION
                     : in.raw_status == 2 ? SG_PRESENCE
                     : SG_EMPTY)
                  : SG_EMPTY;
  SgPipeOut o;
  o.state     = pass;
  o.stable    = pass;
  o.stable_ms = 0;
  o.gated     = in_range;
  return o;
}

void sg_core_init(const SgCoreConfig* cfg) {
  sg_pipe_init();
  g_params = sg_pipe_get_params();
  if (cfg && cfg->default_range_cm) {
    g_params.max_range_cm = clamp_range_cm(cfg->default_range_cm);
  }
  g_pipe_enabled = false;
  g_params_from_nvs = false;
  if (!cfg || cfg->load_from_nvs) {
    sg_config_pipe_load(&g_params, &g_pipe_enabled, &g_params_from_nvs);
  }
  g_params.max_range_cm = clamp_range_cm(g_params.max_range_cm);
  sg_pipe_set_params(&g_params);
  g_snap = {};
  g_snap.pipe = {SG_EMPTY, SG_EMPTY, 0, false};
  g_snap.range_cm = g_params.max_range_cm;
  g_snap.pipe_enabled = g_pipe_enabled;
  sg_pipe_reset_baseline();
  sg_calib_reset();
}

SgCoreSnapshot sg_core_step(const RadarParsed* meas, uint32_t now_ms) {
  if (meas) {
    g_snap.has_meas = true;
    g_snap.meas = *meas;
    g_snap.meas_ms = now_ms;
  }
  if (!g_snap.has_meas) return g_snap;

  bool in_range = (g_snap.meas.distance_cm > 0 && g_snap.meas.distance_cm <= g_params.max_range_cm);
  g_snap.in_range = in_range;

  SgPipeIn in { g_snap.meas.status, g_snap.meas.distance_cm, g_snap.meas.speed_cms, g_snap.meas.snr, now_ms };
  sg_calib_push_sample(&in);

  if (g_pipe_enabled) {
    g_snap.pipe = sg_pipe_step(&in);
  } else {
    g_snap.pipe = passthrough(in, in_range);
  }

  if (!in_range) {
    g_snap.pipe.state  = SG_EMPTY;
    g_snap.pipe.stable = SG_EMPTY;
    g_snap.pipe.gated  = false;
  }

  g_snap.range_cm = g_params.max_range_cm;
  g_snap.pipe_enabled = g_pipe_enabled;
  return g_snap;
}

const SgCoreSnapshot* sg_core_get_snapshot() { return &g_snap; }
const SgParams*       sg_core_get_params()   { return &g_params; }
bool                  sg_core_params_from_nvs() { return g_params_from_nvs; }

void sg_core_set_params(const SgParams* p, bool persist) {
  if (!p) return;
  g_params = *p;
  g_params.max_range_cm = clamp_range_cm(g_params.max_range_cm);
  sg_pipe_set_params(&g_params);
  g_snap.range_cm = g_params.max_range_cm;
  if (persist) sg_config_pipe_save(&g_params, g_pipe_enabled);
}

void sg_core_set_range_cm(uint16_t cm, bool persist) {
  g_params.max_range_cm = clamp_range_cm(cm);
  sg_pipe_set_params(&g_params);
  g_snap.range_cm = g_params.max_range_cm;
  if (persist) sg_config_pipe_save(&g_params, g_pipe_enabled);
}

uint16_t sg_core_get_range_cm() { return g_params.max_range_cm; }

void sg_core_set_pipe_enabled(bool on, bool persist) {
  g_pipe_enabled = on;
  g_snap.pipe_enabled = g_pipe_enabled;
  if (persist) sg_config_pipe_save(&g_params, g_pipe_enabled);
}

bool sg_core_pipe_enabled() { return g_pipe_enabled; }

void sg_core_reset_baseline() {
  sg_pipe_reset_baseline();
}

bool sg_core_calib_start(uint32_t dur_ms) { return sg_calib_start(dur_ms); }
bool sg_core_calib_abort() { return sg_calib_abort(); }
void sg_core_calib_reset() { sg_calib_reset(); }
SgCalibState   sg_core_calib_state() { return sg_calib_state(); }
SgCalibMetrics sg_core_calib_metrics(uint32_t now_ms) { return sg_calib_metrics(now_ms); }
SgCalibSuggest sg_core_calib_suggest() { return sg_calib_build_suggest(&g_params); }

bool sg_core_calib_apply(bool persist) {
  if (!sg_calib_apply(&g_params)) return false;
  g_params.max_range_cm = clamp_range_cm(g_params.max_range_cm);
  sg_pipe_set_params(&g_params);
  g_snap.range_cm = g_params.max_range_cm;
  if (persist) sg_config_pipe_save(&g_params, g_pipe_enabled);
  return true;
}
