#include "sg_calib.h"
#include <string.h>
#include <math.h>

static SgCalibState g_state = SG_CALIB_IDLE;
static uint32_t g_target_ms = 60000;
static uint32_t g_t_start = 0;
static uint32_t g_t_last  = 0;

// Acumuladores de SNR (Welford)
static uint32_t g_samples_total = 0;
static uint32_t g_samples_valid = 0;
static double   g_snr_mean = 0.0;
static double   g_snr_m2   = 0.0;

// Histograma de dist�ncia (bins de 10 cm at� 700 cm)
static const uint16_t HIST_BIN_CM = 10;
static const uint16_t HIST_MAX_CM = 700;
static const int      HIST_BINS   = (HIST_MAX_CM / HIST_BIN_CM) + 1;
static uint32_t g_hist[HIST_BINS];

static void reset_accum() {
  g_t_start = 0;
  g_t_last  = 0;
  g_samples_total = 0;
  g_samples_valid = 0;
  g_snr_mean = 0.0;
  g_snr_m2   = 0.0;
  memset(g_hist, 0, sizeof(g_hist));
}

static uint16_t clamp_range_cm(uint16_t cm) {
  if (cm <= 250) return 200;
  if (cm <= 500) return 400;
  return 600;
}

bool sg_calib_start(uint32_t dur_ms) {
  if (g_state == SG_CALIB_COLLECT) return false;
  g_target_ms = dur_ms ? dur_ms : 60000;
  reset_accum();
  g_state = SG_CALIB_COLLECT;
  return true;
}

SgCalibState sg_calib_state() { return g_state; }

void sg_calib_reset() {
  reset_accum();
  g_state = SG_CALIB_IDLE;
}

bool sg_calib_abort() {
  if (g_state != SG_CALIB_COLLECT) return false;
  sg_calib_reset();
  return true;
}

static void maybe_finish(uint32_t now_ms) {
  if (g_state != SG_CALIB_COLLECT) return;
  if (g_t_start == 0) return;
  if ((now_ms - g_t_start) >= g_target_ms) {
    g_state = SG_CALIB_READY;
  }
}

void sg_calib_push_sample(const SgPipeIn& in) {
  if (g_state != SG_CALIB_COLLECT) return;
  uint32_t now = in.now_ms;
  if (g_t_start == 0) g_t_start = now;
  g_t_last = now;
  g_samples_total++;

  bool valid_dist = (in.dist_cm > 0);
  if (valid_dist) {
    g_samples_valid++;
    float snr = in.snr;
    if (snr < 0.0f) snr = 0.0f;
    if (snr > 1.0f) snr = 1.0f;

    // Welford para m�dia/vari�ncia
    double delta = snr - g_snr_mean;
    g_snr_mean += delta / (double)g_samples_valid;
    double delta2 = snr - g_snr_mean;
    g_snr_m2 += delta * delta2;

    // Hist de dist�ncia
    uint16_t clamped = (in.dist_cm > HIST_MAX_CM) ? HIST_MAX_CM : in.dist_cm;
    int bin = clamped / HIST_BIN_CM;
    if (bin < 0) bin = 0;
    if (bin >= HIST_BINS) bin = HIST_BINS - 1;
    g_hist[bin]++;
  }

  maybe_finish(now);
}

static uint16_t hist_p95() {
  if (g_samples_valid == 0) return 0;
  uint32_t target = (uint32_t)(0.95 * (double)g_samples_valid);
  uint32_t acc = 0;
  for (int i = 0; i < HIST_BINS; ++i) {
    acc += g_hist[i];
    if (acc >= target) {
      uint16_t upper = (uint16_t)((i + 1) * HIST_BIN_CM);
      return upper;
    }
  }
  return HIST_MAX_CM;
}

SgCalibMetrics sg_calib_metrics(uint32_t now_ms) {
  uint32_t ref_now = now_ms ? now_ms : g_t_last;
  maybe_finish(ref_now);

  SgCalibMetrics m;
  m.samples_total = g_samples_total;
  m.samples_valid = g_samples_valid;
  if (g_state == SG_CALIB_IDLE || g_t_start == 0) {
    m.elapsed_ms = 0;
  } else {
    m.elapsed_ms = ref_now - g_t_start;
  }
  m.target_ms = g_target_ms;
  m.progress = (g_target_ms > 0) ? (float)m.elapsed_ms / (float)g_target_ms : 0.0f;
  if (m.progress > 1.0f) m.progress = 1.0f;
  double var = (g_samples_valid > 1) ? (g_snr_m2 / (double)(g_samples_valid - 1)) : 0.0;
  if (var < 0.0) var = 0.0;
  m.snr_mean = (float)g_snr_mean;
  m.snr_std  = (float)sqrt(var);
  m.dist_p95_cm = hist_p95();
  m.valid_ratio = (g_samples_total > 0)
                  ? ((float)g_samples_valid / (float)g_samples_total)
                  : 0.0f;
  m.ready = (g_state == SG_CALIB_READY || g_state == SG_CALIB_APPLIED);
  return m;
}

SgCalibSuggest sg_calib_build_suggest(const SgParams& base) {
  SgCalibMetrics m = sg_calib_metrics(g_t_last);
  SgCalibSuggest s;
  // Valores default iguais ao base (mant�m caso n�o haja dados)
  s.max_range_cm   = base.max_range_cm;
  s.snr_min        = base.snr_min;
  s.delta_exist    = base.delta_exist;
  s.hold_empty_ms  = base.hold_empty_ms;
  s.hold_exist_ms  = base.hold_exist_ms;
  s.hold_motion_ms = base.hold_motion_ms;
  s.k_ema          = base.k_ema;

  if (m.samples_valid == 0) return s;

  // Range sugerido pelo p95
  s.max_range_cm = clamp_range_cm(m.dist_p95_cm);

  float snr_min = (float)(m.snr_mean - 2.0f * m.snr_std);
  if (snr_min < 0.02f) snr_min = 0.02f;
  if (snr_min > 0.5f)  snr_min = 0.5f;
  s.snr_min = snr_min;

  float delta_exist = (float)(m.snr_std * 1.5f);
  if (delta_exist < 0.02f) delta_exist = 0.02f;
  if (delta_exist > 0.30f) delta_exist = 0.30f;
  s.delta_exist = delta_exist;

  // Holds: aumenta se jitter alto, reduz se SNR tranquilo
  bool jitter_high = (m.snr_std > 0.05f);
  s.hold_empty_ms  = (uint16_t)((uint32_t)base.hold_empty_ms + (jitter_high ? 250 : 100));
  s.hold_exist_ms  = (uint16_t)((uint32_t)base.hold_exist_ms + (jitter_high ? 100 : 0));
  s.hold_motion_ms = base.hold_motion_ms;

  // EMA mais lenta se jitter alto
  s.k_ema = jitter_high ? 0.01f : base.k_ema;

  return s;
}

bool sg_calib_apply(SgParams* params_io) {
  if (!params_io) return false;
  if (g_state != SG_CALIB_READY && g_state != SG_CALIB_APPLIED) return false;
  SgCalibSuggest s = sg_calib_build_suggest(*params_io);
  params_io->max_range_cm   = s.max_range_cm;
  params_io->snr_min        = s.snr_min;
  params_io->delta_exist    = s.delta_exist;
  params_io->hold_empty_ms  = s.hold_empty_ms;
  params_io->hold_exist_ms  = s.hold_exist_ms;
  params_io->hold_motion_ms = s.hold_motion_ms;
  params_io->k_ema          = s.k_ema;
  g_state = SG_CALIB_APPLIED;
  return true;
}
