// sketch/SenseGrid/SenseGrid.ino
// SenseGrid — ESP32-C3 + ME73MS01
// Fixes (quiet by default):
//  - Só faz stream PARSED/JSON quando `stream on` (CLI).
//  - TICK em nível DEBUG (LOGD), então só aparece com `log 3`.
//  - cli_help(Print&) / cli_info(Print&) com assinatura certa p/ sg_cli.h
//  - sg_cli_set_handlers com ponteiros de função (sem lambdas erradas)
//  - Range configurável (2/4/6 m) com preset e checksum calculado

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "pins_radar.h"
#include "glue/hal_uart_glue.h"
#include "glue/drv_radar_me73_glue.h"
#include "glue/drv_radar_ld2410c_glue.h"
#include "glue/ring_samples_glue.h"
#include "glue/sg_cli_glue.h"    // sg_cli_set_handlers()/sg_cli_poll()
#include "glue/sg_pipe_glue.h"   // inclui pipeline e helpers
#include "glue/sg_calib_glue.h"  // inclui o assistente de calibração
#include "glue/sg_http_glue.h"   // builders de HTTP/WS
#include "glue/sg_mqtt_glue.h"
#include "glue/sg_net_glue.h"
#include "glue/sg_config_glue.h"
#include "glue/sg_core_glue.h"
#include "sg_core.h"
#include "sg_adapters_logger.h"
#include "components/config/sg_config_pipe.h"
#include "telemetry_ctx.h"
#include "http_service.h"
#include "mqtt_service.h"
#include "components/config/sg_config_profiles.h"
#include "net_service.h"

// ---------------------- Log simples (0=ERR,1=WARN,2=INFO,3=DBG) ----------------------
static int g_log_level = 2;
#define LOGE(...) do{ if(g_log_level>=0){ Serial.printf("[ERROR] "); Serial.printf(__VA_ARGS__); Serial.println(); } }while(0)
#define LOGW(...) do{ if(g_log_level>=1){ Serial.printf("[WARN ] "); Serial.printf(__VA_ARGS__); Serial.println(); } }while(0)
#define LOGI(...) do{ if(g_log_level>=2){ Serial.printf("[INFO ] "); Serial.printf(__VA_ARGS__); Serial.println(); } }while(0)
#define LOGD(...) do{ if(g_log_level>=3){ Serial.printf("[DEBUG] "); Serial.printf(__VA_ARGS__); Serial.println(); } }while(0)

// ---------------------- Globals ----------------------
UartHandle    g_uart;
RadarHandle   g_radar_me_ctx;
Ld2410cHandle g_radar_ld_ctx;
SgRadar       g_radar = { nullptr, nullptr };

enum RadarKind {
  RADAR_ME73 = 0,
  RADAR_LD2410C = 1
};
static RadarKind g_radar_kind = RADAR_ME73;

static SgSample g_samples[256];
SgRing g_ring;

static RadarParsed g_last;
static bool g_has_last = false;
static uint32_t g_last_seen_ms = 0;
static uint32_t g_tick_ms = 0;

static int   g_occ_last = -1;
static bool  g_stream = true;           // liga stream por padrão para logging
static bool  g_stream_json = true;
static unsigned long g_stream_period_ms = 50; // ~20 Hz
static bool g_raw_dump = false;
static uint16_t g_raw_dump_left = 0;

// radar detect/config
static const uint32_t RADAR_BAUD_ME73 = 115200;
static const uint32_t RADAR_BAUD_LD2410C = 256000;
static const uint32_t RADAR_DETECT_TIMEOUT_MS = 1500;
static const uint16_t LD2410C_GATE_CM = 75;
static const uint16_t LD2410C_NO_ONE_S = 3;

// core/pipeline
static SgCoreSnapshot g_core_snap;

static SgTelemetryCtx g_tctx;
static SgNetInfo g_net_info;

// range gate (2.00 m)
// timing
static const uint32_t TICK_PERIOD_MS = 250;
static const uint32_t STALE_MS       = 3000;

// warm-up pós config
static uint32_t g_after_cfg_ms = 0;

// ---------------------- Pipeline helpers/persistência ----------------------
static float clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
}

static void pipe_show(Print& out) {
  SgParams stored = *sg_core_get_params();
  bool enabled = sg_core_pipe_enabled();
  bool has_nvs = false;
  sg_config_pipe_load(&stored, &enabled, &has_nvs);
  out.print(F("{\"enabled\":")); out.print(enabled ? F("true") : F("false"));
  out.print(F(",\"dist_max_cm\":")); out.print(stored.max_range_cm);
  out.print(F(",\"hold_ms\":{\"empty\":")); out.print(stored.hold_empty_ms);
  out.print(F(",\"presence\":")); out.print(stored.hold_exist_ms);
  out.print(F(",\"motion\":")); out.print(stored.hold_motion_ms);
  out.print(F("}"));
  out.print(F(",\"snr_on_exist\":")); out.print(stored.snr_on_exist, 3);
  out.print(F(",\"snr_off_exist\":")); out.print(stored.snr_off_exist, 3);
  out.print(F(",\"snr_min\":")); out.print(stored.snr_min, 3);
  out.print(F(",\"snr_move\":")); out.print(stored.snr_move, 3);
  out.print(F(",\"delta_exist\":")); out.print(stored.delta_exist, 3);
  out.print(F(",\"speed_thr_cms\":")); out.print(stored.speed_thr_cms);
  out.print(F(",\"k_ema\":")); out.print(stored.k_ema, 4);
  out.println('}');
}

// ---------------------- Helpers ----------------------
static const char* status_str(uint8_t s) {
  switch (s) {
    case 0: return "none";
    case 1: return "move";
    case 2: return "exist";
    default: return "?";
  }
}

static const char* radar_kind_str(RadarKind k) {
  switch (k) {
    case RADAR_LD2410C: return "LD2410C";
    case RADAR_ME73:
    default: return "ME73";
  }
}

static void uart_drain_rx(UartHandle* h, uint32_t max_ms=50) {
  if (!h || !h->impl) return;
  HardwareSerial* ser = reinterpret_cast<HardwareSerial*>(h->impl);
  uint32_t t0 = millis();
  while (millis() - t0 < max_ms) {
    while (ser->available() > 0) { ser->read(); }
    delay(2);
  }
}

static bool radar_select_me73() {
  if (!uart_begin(&g_uart, SG_RADAR_UART_NUM, RADAR_BAUD_ME73, SG_RADAR_RX, SG_RADAR_TX)) return false;
  uart_drain_rx(&g_uart, 50);
  g_radar.ops = &SG_RADAR_ME73_OPS;
  g_radar.ctx = &g_radar_me_ctx;
  return sg_radar_begin(&g_radar, &g_uart);
}

static bool radar_select_ld2410c() {
  if (!uart_begin(&g_uart, SG_RADAR_UART_NUM, RADAR_BAUD_LD2410C, SG_RADAR_RX, SG_RADAR_TX)) return false;
  uart_drain_rx(&g_uart, 50);
  g_radar.ops = &SG_RADAR_LD2410C_OPS;
  g_radar.ctx = &g_radar_ld_ctx;
  return sg_radar_begin(&g_radar, &g_uart);
}

static bool radar_try_kind(RadarKind k, uint32_t timeout_ms) {
  if (k == RADAR_LD2410C) {
    if (!radar_select_ld2410c()) return false;
  } else {
    if (!radar_select_me73()) return false;
  }

  uint32_t t0 = millis();
  RadarParsed p;
  while ((millis() - t0) < timeout_ms) {
    if (sg_radar_read_parsed(&g_radar, &p, 80)) return true;
  }
  return false;
}

static bool radar_autodetect() {
  if (radar_try_kind(RADAR_LD2410C, RADAR_DETECT_TIMEOUT_MS)) {
    g_radar_kind = RADAR_LD2410C;
    return true;
  }
  if (radar_try_kind(RADAR_ME73, RADAR_DETECT_TIMEOUT_MS)) {
    g_radar_kind = RADAR_ME73;
    return true;
  }
  // fallback: keep ME73 configured to avoid null ops
  g_radar_kind = RADAR_ME73;
  return radar_select_me73();
}

static void radar_send(const uint8_t* cmd, size_t n) {
  if (!g_uart.impl) return;
  uart_write(&g_uart, cmd, n);
  delay(20);
  uart_drain_rx(&g_uart, 40);
}

// JSON (1 linha)
static void print_json(const RadarParsed& p, const SgPipeOut& o) {
  float dist_m   = p.distance_cm * 0.01f;
  float speed_ms = p.speed_cms   * 0.01f;
  float snr01    = p.snr; // já normalizado (0..1) no driver

  if (g_stream_json) {
    Serial.printf(
      "{\"ts_ms\":%lu,\"status\":\"%s\",\"dist_m\":%.3f,\"speed_mps\":%.3f,"
      "\"snr\":%.3f,\"distance_cm\":%u,\"speed_cms\":%d,\"signal\":%u,"
      "\"az_deg\":%d,\"el_deg\":%d,"
      "\"state\":%d,\"stable\":%d,\"stable_ms\":%lu,\"in_range\":%s}\n",
      (unsigned long)millis(), status_str(p.status),
      dist_m, speed_ms, snr01,
      p.distance_cm, (int)p.speed_cms, p.signal,
      (int)p.azim_deg, (int)p.elev_deg,
      (int)o.state, (int)o.stable, (unsigned long)o.stable_ms,
      (p.distance_cm>0 && p.distance_cm<=sg_core_get_range_cm())? "true":"false"
    );
  } else {
    Serial.printf("[PARSED] status=%s dist=%ucm speed=%d cm/s (%.2f m/s) signal=%u  state=%d stable=%d t=%lu\n",
                  status_str(p.status), p.distance_cm, (int)p.speed_cms, speed_ms, p.signal,
                  (int)o.state, (int)o.stable, (unsigned long)millis());
  }
}

static void print_raw_frame(const RadarRawFrame& rf) {
  Serial.printf("[RAW] len=%u", (unsigned)rf.size);
  for (uint16_t i = 0; i < rf.size; ++i) {
    Serial.printf(" %02X", rf.data[i]);
  }
  Serial.println();
}

// ---------------------- Config do radar no boot ----------------------
// VO hold = 3000 ms (0x0BB8)
static const uint8_t SET_VO_HOLD_3S[]           = {0x55,0x5A,0x00,0x06,0x01,0x80,0x14,0x0B,0xB8,0x0D};
// Save all
static const uint8_t SAVE_ALL[]                 = {0x55,0x5A,0x00,0x04,0x01,0x20,0x04,0xD8};

static void radar_config_boot() {
  if (g_radar_kind == RADAR_ME73) {
    LOGI("[CFG] VO hold = 3000 ms");
    radar_send(SET_VO_HOLD_3S, sizeof(SET_VO_HOLD_3S));

    LOGI("[CFG] Presence max = %u cm", (unsigned)sg_core_get_range_cm());
    radar_set_presence_max(sg_core_get_range_cm());

    LOGI("[CFG] Save all");
    radar_send(SAVE_ALL, sizeof(SAVE_ALL));
  } else if (g_radar_kind == RADAR_LD2410C) {
    LOGI("[CFG] LD2410C boot config");
    radar_set_presence_max(sg_core_get_range_cm());
  }

  g_after_cfg_ms = millis() + 1000; // 1s
}

// ---------------------- CLI local (assinaturas do sg_cli.h) ----------------------
static void cli_help(Print& out) {
  out.println(F("SenseGrid CLI - comandos:"));
  out.println(F("  help            -> esta ajuda"));
  out.println(F("  info            -> info de build e pinos"));
  out.println(F("  stream on|off   -> liga/desliga streaming"));
  out.println(F("  rate <Hz>       -> taxa do stream (0=libera)"));
  out.println(F("  json on|off     -> formato JSON ou texto"));
  out.println(F("  log <0..3>      -> nivel de log (0=err..3=dbg)"));
  out.println(F("  pipe on|off     -> liga/desliga pipeline (persistente)"));
  out.println(F("  pipe show       -> mostra JSON dos parametros (NVS)"));
  out.println(F("  pipe set dist_max <cm>"));
  out.println(F("  pipe set snr_min <0..1>"));
  out.println(F("  pipe set snr_move <0..1>"));
  out.println(F("  pipe set delta_exist <0..1>"));
  out.println(F("  pipe set speed_thr <cm/s>"));
  out.println(F("  pipe set hold presence|motion|empty <ms>"));
  out.println(F("  pipe set k_ema <0.001..0.2>"));
  out.println(F("  range 2|4|6     -> alcance max (m) com preset (stream so emite se in-range)"));
  out.println(F("  calib start|status|apply|abort|reset|preview|profile save|profile load"));
  out.println(F("  raw on|off|once -> dump frame bruto (hex)"));
  out.println(F("  wifi show|set|ap|clear|apply"));
  out.println(F("  wifi set <ssid> <pass>"));
  out.println(F("  wifi ap <ssid> [pass]"));
  out.println(F("  wifi clear [sta|ap|all]"));
  out.println(F("  mqtt show|host <addr>|port <num>|restart"));
}
static void cli_info(Print& out) {
  out.print(F("UART1 RX=")); out.print(SG_RADAR_RX);
  out.print(F(" TX=")); out.print(SG_RADAR_TX);
  uint32_t baud = g_uart.baud ? g_uart.baud : (uint32_t)SG_RADAR_BAUD;
  out.print(F(" @")); out.println(baud);
  out.print(F("OCC pin=")); out.println(SG_PIN_RADAR_OCC);
  out.print(F("Radar=")); out.println(radar_kind_str(g_radar_kind));
  out.print(F("Range (max presence)=")); out.print(sg_core_get_range_cm()); out.println(F(" cm"));
}

static void on_cli_stream(bool enable) { g_stream = enable; LOGI("[CLI] stream %s", enable ? "on" : "off"); }
static void on_cli_rate(unsigned long hz) {
  if (hz == 0) g_stream_period_ms = 0;
  else        g_stream_period_ms = (unsigned long)max(1UL, 1000UL / hz);
  LOGI("[CLI] rate = %lu Hz (period %lu ms)", hz, g_stream_period_ms);
}
static void on_cli_json(bool enable) { g_stream_json = enable; LOGI("[CLI] json %s", enable ? "on" : "off"); }
static void on_cli_log(int lvl) { g_log_level = constrain(lvl, 0, 3); LOGI("[CLI] log level = %d", g_log_level); }

static void on_cli_raw(int argc, char* argv[], Print& out) {
  if (argc < 2) { out.println(F("[raw] uso: raw on|off|once")); return; }
  const char* sub = argv[1];
  if (!strcasecmp(sub, "on")) { g_raw_dump = true; g_raw_dump_left = 0; out.println(F("[raw] on")); return; }
  if (!strcasecmp(sub, "off")) { g_raw_dump = false; g_raw_dump_left = 0; out.println(F("[raw] off")); return; }
  if (!strcasecmp(sub, "once")) { g_raw_dump = false; g_raw_dump_left = 1; out.println(F("[raw] once")); return; }
  out.println(F("[raw] comando invalido"));
}

static void on_cli_wifi(int argc, char* argv[], Print& out) {
  net_service_cli(argc, argv, out);
}

static bool parse_float(const char* s, float& out) {
  if (!s) return false;
  char* end=nullptr;
  out = strtof(s, &end);
  return (end && *end == 0);
}

static bool parse_uint(const char* s, uint32_t& out) {
  if (!s) return false;
  char* end=nullptr;
  unsigned long v = strtoul(s, &end, 10);
  if (!(end && *end == 0)) return false;
  out = (uint32_t)v;
  return true;
}

static void on_cli_pipe(int argc, char* argv[], Print& out) {
  if (argc < 2) { out.println(F("[pipe] argumentos insuficientes")); return; }
  const char* sub = argv[1];

  if (!strcasecmp(sub, "on") || !strcasecmp(sub, "off")) {
    bool on = !strcasecmp(sub, "on");
    sg_core_set_pipe_enabled(on, true);
    out.printf("[pipe] %s\n", on ? "on" : "off");
    return;
  }

  if (!strcasecmp(sub, "show")) {
    pipe_show(out);
    return;
  }

  if (!strcasecmp(sub, "set")) {
    if (argc < 4) { out.println(F("[pipe] uso: pipe set <param> <valor>")); return; }
    const char* key = argv[2];
    const char* val = argv[3];
    bool changed = true;
    bool range_changed = false;
    SgParams params = *sg_core_get_params();

    if (!strcasecmp(key, "dist_max")) {
      uint32_t v=0; if (!parse_uint(val, v)) { out.println(F("[pipe] dist_max invalido")); return; }
      params.max_range_cm = (uint16_t)v;
      range_changed = true;
    } else if (!strcasecmp(key, "snr_min")) {
      float v=0; if (!parse_float(val, v)) { out.println(F("[pipe] snr_min invalido")); return; }
      params.snr_min = clamp01(v);
    } else if (!strcasecmp(key, "snr_move")) {
      float v=0; if (!parse_float(val, v)) { out.println(F("[pipe] snr_move invalido")); return; }
      params.snr_move = clamp01(v);
    } else if (!strcasecmp(key, "delta_exist")) {
      float v=0; if (!parse_float(val, v)) { out.println(F("[pipe] delta_exist invalido")); return; }
      params.delta_exist = constrain(v, 0.0f, 1.0f);
    } else if (!strcasecmp(key, "speed_thr")) {
      uint32_t v=0; if (!parse_uint(val, v)) { out.println(F("[pipe] speed_thr invalido")); return; }
      params.speed_thr_cms = (uint16_t)min<uint32_t>(v, 2000);
    } else if (!strcasecmp(key, "hold") && argc >= 5) {
      const char* which = argv[3];
      const char* vstr  = argv[4];
      uint32_t v=0; if (!parse_uint(vstr, v)) { out.println(F("[pipe] hold invalido")); return; }
      if (!strcasecmp(which, "presence")) params.hold_exist_ms  = (uint16_t)min<uint32_t>(v, 10000);
      else if (!strcasecmp(which, "motion")) params.hold_motion_ms = (uint16_t)min<uint32_t>(v, 10000);
      else if (!strcasecmp(which, "empty"))  params.hold_empty_ms  = (uint16_t)min<uint32_t>(v, 20000);
      else { out.println(F("[pipe] hold alvo invalido (presence|motion|empty)")); return; }
    } else if (!strcasecmp(key, "k_ema")) {
      float v=0; if (!parse_float(val, v)) { out.println(F("[pipe] k_ema invalido")); return; }
      params.k_ema = constrain(v, 0.001f, 0.2f);
    } else {
      changed = false;
    }

    if (!changed) { out.println(F("[pipe] chave desconhecida")); return; }
    sg_core_set_params(&params, true);
    if (range_changed) {
      sg_core_reset_baseline();
      radar_set_presence_max(sg_core_get_range_cm());
      g_after_cfg_ms = millis() + 1000;
    }
    out.println(F("[pipe] ok"));
    return;
  }

  out.println(F("[pipe] comando invalido"));
}

// ---------------------- Assistente de calibração ----------------------
static const char* calib_state_str(SgCalibState st) {
  switch (st) {
    case SG_CALIB_IDLE: return "idle";
    case SG_CALIB_COLLECT: return "collecting";
    case SG_CALIB_READY: return "ready";
    case SG_CALIB_APPLIED: return "applied";
    default: return "?";
  }
}

static void print_calib_status(Print& out) {
  SgCalibMetrics m = sg_core_calib_metrics(millis());
  SgCalibSuggest s = sg_core_calib_suggest();
  const SgParams* cur = sg_core_get_params();
  // opcional: log estruturado para calib_report.py consumir
  Serial.printf("{\"tag\":\"calib\",\"meta\":{\"ts_ms\":%lu,\"state\":\"%s\"},\"metrics\":{\"elapsed_ms\":%u,\"valid_ratio\":%.3f,\"snr_mean\":%.4f,\"snr_std\":%.4f,\"dist_p95_cm\":%u},\"current\":{\"max_range_cm\":%u,\"hold_empty_ms\":%u},\"suggest\":{\"max_range_cm\":%u,\"hold_empty_ms\":%u}}\n",
    (unsigned long)m.elapsed_ms,
    calib_state_str(sg_core_calib_state()),
    (unsigned)m.elapsed_ms,
    m.valid_ratio,
    m.snr_mean,
    m.snr_std,
    m.dist_p95_cm,
    cur->max_range_cm,
    cur->hold_empty_ms,
    s.max_range_cm,
    s.hold_empty_ms
  );
  out.print(F("{\"state\":\"")); out.print(calib_state_str(sg_core_calib_state())); out.print('"');
  out.print(F(",\"elapsed_ms\":")); out.print(m.elapsed_ms);
  out.print(F(",\"target_ms\":")); out.print(m.target_ms);
  out.print(F(",\"progress\":")); out.print(m.progress, 3);
  out.print(F(",\"samples\":")); out.print(m.samples_total);
  out.print(F(",\"valid\":")); out.print(m.samples_valid);
  out.print(F(",\"valid_ratio\":")); out.print(m.valid_ratio, 3);
  out.print(F(",\"snr_mean\":")); out.print(m.snr_mean, 4);
  out.print(F(",\"snr_std\":")); out.print(m.snr_std, 4);
  out.print(F(",\"dist_p95_cm\":")); out.print(m.dist_p95_cm);
  out.print(F(",\"suggest\":{"));
  out.print(F("\"max_range_cm\":")); out.print(s.max_range_cm);
  out.print(F(",\"snr_min\":")); out.print(s.snr_min, 3);
  out.print(F(",\"delta_exist\":")); out.print(s.delta_exist, 3);
  out.print(F(",\"hold_empty_ms\":")); out.print(s.hold_empty_ms);
  out.print(F(",\"hold_exist_ms\":")); out.print(s.hold_exist_ms);
  out.print(F(",\"hold_motion_ms\":")); out.print(s.hold_motion_ms);
  out.print(F(",\"k_ema\":")); out.print(s.k_ema, 4);
  out.print('}');
  out.println('}');
}

static bool calib_profile_save(const char* name) {
  return sg_config_profile_save(name, sg_core_get_params());
}

static bool calib_profile_load(const char* name, SgParams* out) {
  return sg_config_profile_load(name, out);
}

static void calib_preview(Print& out, const SgParams& cur, const SgCalibSuggest& sug) {
  out.print(F("{\"current\":{"));
  out.print(F("\"max_range_cm\":")); out.print(cur.max_range_cm);
  out.print(F(",\"snr_min\":")); out.print(cur.snr_min, 3);
  out.print(F(",\"delta_exist\":")); out.print(cur.delta_exist, 3);
  out.print(F(",\"hold_empty_ms\":")); out.print(cur.hold_empty_ms);
  out.print(F(",\"hold_exist_ms\":")); out.print(cur.hold_exist_ms);
  out.print(F(",\"hold_motion_ms\":")); out.print(cur.hold_motion_ms);
  out.print(F(",\"k_ema\":")); out.print(cur.k_ema, 4);
  out.print(F("},\"suggest\":{"));
  out.print(F("\"max_range_cm\":")); out.print(sug.max_range_cm);
  out.print(F(",\"snr_min\":")); out.print(sug.snr_min, 3);
  out.print(F(",\"delta_exist\":")); out.print(sug.delta_exist, 3);
  out.print(F(",\"hold_empty_ms\":")); out.print(sug.hold_empty_ms);
  out.print(F(",\"hold_exist_ms\":")); out.print(sug.hold_exist_ms);
  out.print(F(",\"hold_motion_ms\":")); out.print(sug.hold_motion_ms);
  out.print(F(",\"k_ema\":")); out.print(sug.k_ema, 4);
  out.print(F("}}"));
  out.println();
}

static void on_cli_calib(int argc, char* argv[], Print& out) {
  if (argc < 2) {
    out.println(F("[calib] uso: calib start [ms] | status | apply | abort | reset | factory | preview | rollback | profile save <name> | profile load <name>"));
    return;
  }
  const char* sub = argv[1];

  if (!strcasecmp(sub, "start")) {
    uint32_t dur = 60000;
    if (argc >= 3) {
      if (!parse_uint(argv[2], dur)) { out.println(F("[calib] duracao invalida")); return; }
    }
    bool ok = sg_core_calib_start(dur);
    if (!ok) {
      out.println(F("[calib] ja coletando; use calib status/apply/reset"));
    } else {
      out.printf("[calib] collecting for %lu ms\n", (unsigned long)dur);
    }
    return;
  }

  if (!strcasecmp(sub, "status")) {
    print_calib_status(out);
    return;
  }

  if (!strcasecmp(sub, "preview")) {
    SgCalibSuggest s = sg_core_calib_suggest();
    calib_preview(out, *sg_core_get_params(), s);
    return;
  }

  if (!strcasecmp(sub, "abort")) {
    bool ok = sg_core_calib_abort();
    out.println(ok ? F("[calib] abortado") : F("[calib] nada para abortar"));
    return;
  }

  if (!strcasecmp(sub, "apply")) {
    if (sg_core_calib_state() != SG_CALIB_READY && sg_core_calib_state() != SG_CALIB_APPLIED) {
      out.println(F("[calib] nada para aplicar (run calib start e aguarde)"));
      return;
    }
    SgCalibSuggest sug = sg_core_calib_suggest();
    calib_preview(out, *sg_core_get_params(), sug);
    uint16_t old_range = sg_core_get_range_cm();
    bool ok = sg_core_calib_apply(true);
    if (!ok) { out.println(F("[calib] apply falhou")); return; }
    sg_core_reset_baseline();
    if (sg_core_get_range_cm() != old_range) {
      radar_set_presence_max(sg_core_get_range_cm());
      g_after_cfg_ms = millis() + 1000;
    }
    out.println(F("[calib] applied and persisted"));
    print_calib_status(out);
    return;
  }

  if (!strcasecmp(sub, "profile") && argc >= 4) {
    const char* op = argv[2];
    const char* name = argv[3];
    if (!strcasecmp(op, "save")) {
      if (!calib_profile_save(name)) { out.println(F("[calib] profile save falhou (nome invalido ou NVS)")); return; }
      out.printf("[calib] profile '%s' salvo\n", name);
      return;
    }
    if (!strcasecmp(op, "load")) {
      SgParams loaded;
      if (!calib_profile_load(name, &loaded)) { out.println(F("[calib] profile load falhou (nao existe?)")); return; }
      sg_core_set_params(&loaded, true);
      sg_core_reset_baseline();
      radar_set_presence_max(sg_core_get_range_cm());
      g_after_cfg_ms = millis() + 1000;
      out.printf("[calib] profile '%s' aplicado e persistido\n", name);
      pipe_show(out);
      return;
    }
  }

  if (!strcasecmp(sub, "reset")) {
    sg_core_calib_reset();
    out.println(F("[calib] reset ok (coleta)"));
    return;
  }

  if (!strcasecmp(sub, "factory")) {
    sg_pipe_init();
    SgParams def = sg_pipe_get_params();
    sg_core_set_pipe_enabled(true, true);
    sg_core_set_params(&def, true);
    sg_core_reset_baseline();
    radar_set_presence_max(def.max_range_cm);
    g_after_cfg_ms = millis() + 1000;
    out.println(F("[calib] factory reset: params default aplicados e persistidos"));
    return;
  }

  if (!strcasecmp(sub, "rollback")) {
    SgParams bak = *sg_core_get_params();
    bool en = sg_core_pipe_enabled();
    if (!sg_config_pipe_restore_backup(&bak, &en)) {
      out.println(F("[calib] rollback indisponivel (sem backup)"));
      return;
    }
    sg_core_set_pipe_enabled(en, true);
    sg_core_set_params(&bak, true);
    sg_core_reset_baseline();
    radar_set_presence_max(bak.max_range_cm);
    g_after_cfg_ms = millis() + 1000;
    out.println(F("[calib] rollback aplicado a partir do backup"));
    pipe_show(out);
    return;
  }

  out.println(F("[calib] comando invalido"));
}

static void radar_set_presence_max(uint16_t cm) {
  if (g_radar_kind == RADAR_ME73) {
    uint8_t frame[10];
    frame[0] = 0x55;
    frame[1] = 0x5A;
    frame[2] = 0x00;
    frame[3] = 0x06; // LEN (FUNC..SUM) = 6 bytes
    frame[4] = 0x01;
    frame[5] = 0x80;
    frame[6] = 0x0E;
    frame[7] = (uint8_t)((cm >> 8) & 0xFF);
    frame[8] = (uint8_t)(cm & 0xFF);
    uint8_t sum = (uint8_t)((frame[4] + frame[5] + frame[6] + frame[7] + frame[8]) & 0xFF); // vendor checksum
    frame[9] = sum;
    LOGI("[CFG] Presence max = %u cm (sum=0x%02X)", (unsigned)cm, (unsigned)sum);
    radar_send(frame, sizeof(frame));
    return;
  }

  if (g_radar_kind == RADAR_LD2410C) {
    uint16_t gate = (uint16_t)((cm + (LD2410C_GATE_CM - 1)) / LD2410C_GATE_CM);
    if (gate < 2) gate = 2;
    if (gate > 8) gate = 8;
    LOGI("[CFG] LD2410C max gate = %u (cm=%u)", (unsigned)gate, (unsigned)cm);
    if (!ld2410c_cmd_enable(&g_uart)) LOGW("[CFG] LD2410C enable cmd failed");
    delay(20);
    if (!ld2410c_cmd_set_max_gates(&g_uart, (uint8_t)gate, (uint8_t)gate, LD2410C_NO_ONE_S)) {
      LOGW("[CFG] LD2410C set max gates failed");
    }
    delay(20);
    if (!ld2410c_cmd_end(&g_uart)) LOGW("[CFG] LD2410C end cmd failed");
    uart_drain_rx(&g_uart, 50);
  }
}

static void on_range_applied(uint16_t cm) {
  radar_set_presence_max(cm);
  g_after_cfg_ms = millis() + 1000;
}

static void on_cli_range(uint32_t cm) {
  if (cm > 0 && cm <= 10) cm *= 100; // aceita metros (2/4/6)
  sg_core_set_range_cm((uint16_t)cm, true);
  sg_core_reset_baseline();
  radar_set_presence_max(sg_core_get_range_cm());
  g_after_cfg_ms = millis() + 1000;
  LOGI("[CLI] range set to %u cm (preset)", (unsigned)sg_core_get_range_cm());
}

// ---------------------- Setup/Loop ----------------------
void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 1500) { /* aguarda enumerar */ }

  net_service_init(&g_net_info);

  sg_telemetry_init(&g_tctx, 1);
  sg_adapters_logger_init();
  sg_adapters_logger_set_device_id(sg_telemetry_device_id(&g_tctx), 1);
  http_service_init(&g_tctx, &g_net_info);
  mqtt_service_init(&g_tctx, on_range_applied);


  pinMode(SG_PIN_RADAR_OCC, INPUT_PULLDOWN);

  if (!radar_autodetect()) {
    LOGE("[ERR] radar autodetect failed");
  }
  LOGI("[RADAR] %s @%lu", radar_kind_str(g_radar_kind), (unsigned long)g_uart.baud);

  ring_init(&g_ring, g_samples, 256);
  SgCoreConfig core_cfg;
  core_cfg.default_range_cm = 200;
  core_cfg.load_from_nvs = true;
  sg_core_init(&core_cfg);
  g_core_snap = *sg_core_get_snapshot();

  LOGI("[CFG] stream default=on (use 'stream off' para silenciar)");

  Serial.println();
  LOGI("[BOOT] SenseGrid + Pipeline + CLI");
  Serial.printf("[UART1] RX=%d TX=%d @%lu\n", SG_RADAR_RX, SG_RADAR_TX, (unsigned long)g_uart.baud);
  Serial.printf("[OCC] pin=%d\n", SG_PIN_RADAR_OCC);

  radar_config_boot();

  // Handlers do CLI (ponteiros p/ função com assinatura certa)
  sg_cli_set_handlers(
    /*help  */ cli_help,
    /*info  */ cli_info,
    /*stream*/ on_cli_stream,
    /*rate  */ on_cli_rate,
    /*json  */ on_cli_json,
    /*log   */ on_cli_log,
    /*pipe  */ on_cli_pipe,
    /*range */ on_cli_range,
    /*calib */ on_cli_calib,
    /*raw   */ on_cli_raw,
    /*mqtt  */ mqtt_service_cli,
    /*wifi  */ on_cli_wifi
  );

}

void loop() {
  // 0) CLI (USB CDC) — usa o mesmo Serial para in/out
  sg_cli_poll(Serial);

  // 1) OCC digital (log de borda)
  int occ = digitalRead(SG_PIN_RADAR_OCC);
  if (occ != g_occ_last) {
    g_occ_last = occ;
    LOGI("[OCC] %d  t=%lu", occ, (unsigned long)millis());
  }

  // 2) pular leituras durante warm-up
  if (millis() >= g_after_cfg_ms) {
    // tenta até 3 frames por loop
    uint32_t read_timeout = (g_radar_kind == RADAR_LD2410C) ? 80 : 20;
    for (int i = 0; i < 3; ++i) {
      RadarParsed p;
      if (!sg_radar_read_parsed(&g_radar, &p, read_timeout)) break;

      g_last = p;
      g_has_last = true;
      g_last_seen_ms = millis();

      bool dump_now = (g_raw_dump || g_raw_dump_left > 0);
      if (dump_now) {
        RadarRawFrame rf;
        if (sg_radar_get_last_raw(&g_radar, &rf)) {
          print_raw_frame(rf);
        }
        if (g_raw_dump_left > 0) g_raw_dump_left--;
      }

      g_core_snap = sg_core_step(&p, g_last_seen_ms);
      sg_adapters_logger_on_measurement(&g_core_snap);

      // ring buffer
      SgSample s {
        .t_ms = g_last_seen_ms,
        .t_us = micros(),
        .distance_cm = p.distance_cm,
        .speed_cms   = p.speed_cms,
        .signal      = p.signal,
        .status      = p.status,
        ._rsv        = 0,
      };
      ring_push(&g_ring, &s);

      // << Quieto por padrão: só imprime quando stream estiver ON >>
      if (g_stream && g_core_snap.in_range) {
        // respeita período se configurado
        static unsigned long last_emit = 0;
        unsigned long now = millis();
        if (g_stream_period_ms == 0 || (now - last_emit) >= g_stream_period_ms) {
          print_json(p, g_core_snap.pipe);
          last_emit = now;
        }
      }
      mqtt_service_on_measurement(&g_core_snap);
    }
  }

  // 3) ticker — se ficar stale (>3s) e OCC=1, mostra "exist" (apenas em DEBUG)
  if (millis() - g_tick_ms >= TICK_PERIOD_MS) {
    g_tick_ms = millis();
    if (g_has_last) {
      bool stale = (millis() - g_last_seen_ms > STALE_MS);
      uint8_t shown = stale ? ((digitalRead(SG_PIN_RADAR_OCC)==1) ? 2 : 0) : g_last.status;
      LOGD("[TICK] status=%s dist=%ucm signal=%u  t=%lu",
           status_str(shown), g_last.distance_cm, g_last.signal, (unsigned long)millis());
    } else {
      LOGD("[TICK] aguardando primeira leitura...");
    }
  }

  // 4) dreno ring (placeholder)
  SgSample out;
  (void)ring_pop(&g_ring, &out);
  // 5) HTTP/WS
  http_service_loop(&g_core_snap);

  // 6) MQTT
  mqtt_service_loop();

  delay(2);
}
