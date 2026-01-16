#include "sg_core.h"
#include <string.h>
#include "../config/sg_config_pipe.h"

// Estado interno
static SgParams       g_params;
static bool           g_pipe_enabled = false;
static bool           g_params_from_nvs = false;
static SgCoreSnapshot g_snap;
static SgCoreKpi      g_kpi_last;
static bool           g_kpi_ready = false;

typedef struct {
  uint32_t window_ms;
  uint32_t window_start_ms;
  uint32_t last_meas_ms;
  uint32_t meas_total;
  uint32_t meas_valid;
  double   snr_sum;
  uint32_t snr_count;
  uint32_t latency_sum_ms;
  uint32_t latency_count;
  uint32_t latency_max_ms;
  uint32_t state_count[3];
  uint32_t transitions;
  SgState  last_state;
  bool     has_last_state;
  uint32_t fp_count;
  uint32_t fn_count;
  uint32_t fpfn_total;
} SgKpiAgg;

static SgKpiAgg g_kpi;
static const uint32_t KPI_WINDOW_DEFAULT_MS = 60000;

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

static SgState map_raw_state(uint8_t raw) {
  switch (raw) {
    case 1: return SG_MOTION;
    case 2: return SG_PRESENCE;
    default: return SG_EMPTY;
  }
}

static void kpi_reset_window(uint32_t now_ms, bool keep_state, SgState state) {
  uint32_t window_ms = g_kpi.window_ms ? g_kpi.window_ms : KPI_WINDOW_DEFAULT_MS;
  SgKpiAgg fresh;
  memset(&fresh, 0, sizeof(fresh));
  fresh.window_ms = window_ms;
  fresh.window_start_ms = now_ms;
  fresh.last_meas_ms = now_ms;
  if (keep_state) {
    fresh.last_state = state;
    fresh.has_last_state = true;
  }
  g_kpi = fresh;
}

static void kpi_push(const RadarParsed* meas, const SgPipeOut* pipe, bool in_range, uint32_t now_ms) {
  if (!meas || !pipe) return;
  if (g_kpi.window_ms == 0) return;
  if (g_kpi.window_start_ms == 0) g_kpi.window_start_ms = now_ms;

  g_kpi.meas_total++;
  if (in_range) g_kpi.meas_valid++;

  float snr = meas->snr;
  if (snr < 0.0f) snr = 0.0f;
  if (snr > 1.0f) snr = 1.0f;
  if (in_range) {
    g_kpi.snr_sum += (double)snr;
    g_kpi.snr_count++;
  }

  if (g_kpi.last_meas_ms > 0) {
    uint32_t dt = (uint32_t)(now_ms - g_kpi.last_meas_ms);
    g_kpi.latency_sum_ms += dt;
    g_kpi.latency_count++;
    if (dt > g_kpi.latency_max_ms) g_kpi.latency_max_ms = dt;
  }
  g_kpi.last_meas_ms = now_ms;

  SgState obs_state = pipe->stable;
  uint8_t idx = (uint8_t)obs_state;
  if (idx < 3) g_kpi.state_count[idx]++;

  if (g_kpi.has_last_state && obs_state != g_kpi.last_state) {
    g_kpi.transitions++;
  }
  g_kpi.last_state = obs_state;
  g_kpi.has_last_state = true;

  if (in_range) {
    SgState raw_state = map_raw_state(meas->status);
    if (raw_state == SG_EMPTY && obs_state != SG_EMPTY) g_kpi.fp_count++;
    else if (raw_state != SG_EMPTY && obs_state == SG_EMPTY) g_kpi.fn_count++;
    g_kpi.fpfn_total++;
  }

  if (g_kpi.window_ms > 0 && (now_ms - g_kpi.window_start_ms) >= g_kpi.window_ms) {
    SgCoreKpi out;
    memset(&out, 0, sizeof(out));
    out.window_ms = g_kpi.window_ms;
    out.window_start_ms = g_kpi.window_start_ms;
    out.window_end_ms = now_ms;
    out.meas_total = g_kpi.meas_total;
    out.meas_valid = g_kpi.meas_valid;
    out.snr_avg = (g_kpi.snr_count > 0) ? (float)(g_kpi.snr_sum / (double)g_kpi.snr_count) : 0.0f;
    out.latency_avg_ms = (g_kpi.latency_count > 0) ? (g_kpi.latency_sum_ms / g_kpi.latency_count) : 0;
    out.latency_max_ms = g_kpi.latency_max_ms;
    if (g_kpi.meas_total > 0) {
      float total = (float)g_kpi.meas_total;
      out.state_ratio_empty = (float)g_kpi.state_count[SG_EMPTY] / total;
      out.state_ratio_presence = (float)g_kpi.state_count[SG_PRESENCE] / total;
      out.state_ratio_motion = (float)g_kpi.state_count[SG_MOTION] / total;
    }
    out.transition_count = g_kpi.transitions;
    if (g_kpi.fpfn_total > 0) {
      float denom = (float)g_kpi.fpfn_total;
      out.fp_proxy_ratio = (float)g_kpi.fp_count / denom;
      out.fn_proxy_ratio = (float)g_kpi.fn_count / denom;
    }
    g_kpi_last = out;
    g_kpi_ready = true;
    kpi_reset_window(now_ms, true, obs_state);
  }
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
  memset(&g_kpi_last, 0, sizeof(g_kpi_last));
  g_kpi.window_ms = KPI_WINDOW_DEFAULT_MS;
  kpi_reset_window(0, false, SG_EMPTY);
  g_kpi_ready = false;
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
  if (meas) {
    kpi_push(&g_snap.meas, &g_snap.pipe, g_snap.in_range, now_ms);
  }
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

void sg_core_kpi_reset() {
  memset(&g_kpi_last, 0, sizeof(g_kpi_last));
  kpi_reset_window(0, false, SG_EMPTY);
  g_kpi_ready = false;
}

void sg_core_kpi_set_window_ms(uint32_t window_ms) {
  if (window_ms == 0) window_ms = KPI_WINDOW_DEFAULT_MS;
  if (window_ms < 1000) window_ms = 1000;
  g_kpi.window_ms = window_ms;
  SgState st = g_snap.has_meas ? g_snap.pipe.stable : SG_EMPTY;
  kpi_reset_window(g_snap.has_meas ? g_snap.meas_ms : 0, g_snap.has_meas, st);
  g_kpi_ready = false;
}

uint32_t sg_core_kpi_window_ms() {
  return g_kpi.window_ms ? g_kpi.window_ms : KPI_WINDOW_DEFAULT_MS;
}

bool sg_core_kpi_poll(SgCoreKpi* out) {
  if (!g_kpi_ready) return false;
  if (out) *out = g_kpi_last;
  g_kpi_ready = false;
  return true;
}

const SgCoreKpi* sg_core_kpi_last() {
  return &g_kpi_last;
}
