#include "sg_config_pipe.h"
#include <Preferences.h>

static const char* NAMESPACE_PIPE = "pipe";
static const uint32_t PIPE_CFG_VER = 1;

// Helpers locais
static uint16_t load_u16(Preferences& prefs, const char* key, uint16_t defv) {
  return (uint16_t)prefs.getUInt(key, defv);
}
static float load_f32(Preferences& prefs, const char* key, float defv) {
  return prefs.getFloat(key, defv);
}

void sg_config_pipe_load(SgParams* params_io, bool* enabled_io, bool* has_nvs_out) {
  if (!params_io) return;
  bool enabled = enabled_io ? *enabled_io : false;
  bool has_nvs = false;

  Preferences prefs;
  prefs.begin(NAMESPACE_PIPE, true);
  uint32_t ver = prefs.getUInt("ver", 0);
  has_nvs = (ver == PIPE_CFG_VER) && prefs.isKey("dist_max");
  if (has_nvs) {
    params_io->max_range_cm   = load_u16(prefs, "dist_max", params_io->max_range_cm);
    params_io->hold_empty_ms  = load_u16(prefs, "hold_e",  params_io->hold_empty_ms);
    params_io->hold_exist_ms  = load_u16(prefs, "hold_p",  params_io->hold_exist_ms);
    params_io->hold_motion_ms = load_u16(prefs, "hold_m",  params_io->hold_motion_ms);
    params_io->snr_on_exist   = load_f32(prefs, "snr_on",  params_io->snr_on_exist);
    params_io->snr_off_exist  = load_f32(prefs, "snr_off", params_io->snr_off_exist);
    params_io->snr_min        = load_f32(prefs, "snr_min", params_io->snr_min);
    params_io->snr_move       = load_f32(prefs, "snr_mov", params_io->snr_move);
    params_io->delta_exist    = load_f32(prefs, "delta_ex",params_io->delta_exist);
    params_io->speed_thr_cms  = load_u16(prefs, "spd_thr", params_io->speed_thr_cms);
    params_io->k_ema          = load_f32(prefs, "k_ema",   params_io->k_ema);
    enabled = prefs.getBool("enabled", enabled);
  }
  prefs.end();

  if (enabled_io) *enabled_io = enabled;
  if (has_nvs_out) *has_nvs_out = has_nvs;
}

void sg_config_pipe_save(const SgParams* params, bool enabled) {
  if (!params) return;
  Preferences prefs;
  prefs.begin(NAMESPACE_PIPE, false);

  // Backup se já houver config
  if (prefs.isKey("ver")) {
    prefs.putBool("b_enabled", prefs.getBool("enabled", enabled));
    prefs.putUInt("b_dist_max", prefs.getUInt("dist_max", params->max_range_cm));
    prefs.putUInt("b_hold_e",   prefs.getUInt("hold_e",  params->hold_empty_ms));
    prefs.putUInt("b_hold_p",   prefs.getUInt("hold_p",  params->hold_exist_ms));
    prefs.putUInt("b_hold_m",   prefs.getUInt("hold_m",  params->hold_motion_ms));
    prefs.putFloat("b_snr_on",  prefs.getFloat("snr_on", params->snr_on_exist));
    prefs.putFloat("b_snr_off", prefs.getFloat("snr_off",params->snr_off_exist));
    prefs.putFloat("b_snr_min", prefs.getFloat("snr_min",params->snr_min));
    prefs.putFloat("b_snr_mov", prefs.getFloat("snr_mov",params->snr_move));
    prefs.putFloat("b_delta_ex",prefs.getFloat("delta_ex",params->delta_exist));
    prefs.putUInt("b_spd_thr",  prefs.getUInt("spd_thr", params->speed_thr_cms));
    prefs.putFloat("b_k_ema",   prefs.getFloat("k_ema",  params->k_ema));
  }

  prefs.putUInt("ver", PIPE_CFG_VER);
  prefs.putBool("enabled", enabled);
  prefs.putUInt("dist_max", params->max_range_cm);
  prefs.putUInt("hold_e",   params->hold_empty_ms);
  prefs.putUInt("hold_p",   params->hold_exist_ms);
  prefs.putUInt("hold_m",   params->hold_motion_ms);
  prefs.putFloat("snr_on",  params->snr_on_exist);
  prefs.putFloat("snr_off", params->snr_off_exist);
  prefs.putFloat("snr_min", params->snr_min);
  prefs.putFloat("snr_mov", params->snr_move);
  prefs.putFloat("delta_ex",params->delta_exist);
  prefs.putUInt("spd_thr",  params->speed_thr_cms);
  prefs.putFloat("k_ema",   params->k_ema);
  prefs.end();
}

bool sg_config_pipe_restore_backup(SgParams* params_io, bool* enabled_io) {
  if (!params_io) return false;
  bool enabled = enabled_io ? *enabled_io : false;
  Preferences prefs;
  prefs.begin(NAMESPACE_PIPE, true);
  if (!prefs.isKey("b_dist_max")) { prefs.end(); return false; }
  enabled = prefs.getBool("b_enabled", enabled);
  params_io->max_range_cm   = load_u16(prefs, "b_dist_max", params_io->max_range_cm);
  params_io->hold_empty_ms  = load_u16(prefs, "b_hold_e",   params_io->hold_empty_ms);
  params_io->hold_exist_ms  = load_u16(prefs, "b_hold_p",   params_io->hold_exist_ms);
  params_io->hold_motion_ms = load_u16(prefs, "b_hold_m",   params_io->hold_motion_ms);
  params_io->snr_on_exist   = load_f32(prefs, "b_snr_on",   params_io->snr_on_exist);
  params_io->snr_off_exist  = load_f32(prefs, "b_snr_off",  params_io->snr_off_exist);
  params_io->snr_min        = load_f32(prefs, "b_snr_min",  params_io->snr_min);
  params_io->snr_move       = load_f32(prefs, "b_snr_mov",  params_io->snr_move);
  params_io->delta_exist    = load_f32(prefs, "b_delta_ex", params_io->delta_exist);
  params_io->speed_thr_cms  = load_u16(prefs, "b_spd_thr",  params_io->speed_thr_cms);
  params_io->k_ema          = load_f32(prefs, "b_k_ema",    params_io->k_ema);
  prefs.end();
  if (enabled_io) *enabled_io = enabled;
  return true;
}
