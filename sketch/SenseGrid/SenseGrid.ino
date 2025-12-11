// sketch/SenseGrid/SenseGrid.ino
// SenseGrid — ESP32-C3 + ME73MS01
// Fixes (quiet by default):
//  - Só faz stream PARSED/JSON quando `stream on` (CLI).
//  - TICK em nível DEBUG (LOGD), então só aparece com `log 3`.
//  - cli_help(Print&) / cli_info(Print&) com assinatura certa p/ sg_cli.h
//  - sg_cli_set_handlers com ponteiros de função (sem lambdas erradas)
//  - Range configurável (2/4/6 m) com preset e checksum calculado

#include <Arduino.h>
#include <Preferences.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <WiFi.h>
#include <WebServer.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha1.h>

#include "pins_radar.h"
#include "glue/hal_uart_glue.h"
#include "glue/drv_radar_me73_glue.h"
#include "glue/ring_samples_glue.h"
#include "glue/sg_cli_glue.h"    // sg_cli_set_handlers()/sg_cli_poll()
#include "glue/sg_pipe_glue.h"   // inclui pipeline e helpers
#include "glue/sg_calib_glue.h"  // inclui o assistente de calibração
#include "glue/sg_http_glue.h"   // builders de HTTP/WS
#include "glue/sg_mqtt_glue.h"

// ---------------------- Log simples (0=ERR,1=WARN,2=INFO,3=DBG) ----------------------
static int g_log_level = 2;
#define LOGE(...) do{ if(g_log_level>=0){ Serial.printf("[ERROR] "); Serial.printf(__VA_ARGS__); Serial.println(); } }while(0)
#define LOGW(...) do{ if(g_log_level>=1){ Serial.printf("[WARN ] "); Serial.printf(__VA_ARGS__); Serial.println(); } }while(0)
#define LOGI(...) do{ if(g_log_level>=2){ Serial.printf("[INFO ] "); Serial.printf(__VA_ARGS__); Serial.println(); } }while(0)
#define LOGD(...) do{ if(g_log_level>=3){ Serial.printf("[DEBUG] "); Serial.printf(__VA_ARGS__); Serial.println(); } }while(0)

// ---------------------- Globals ----------------------
UartHandle  g_uart;
RadarHandle g_radar;

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

// pipeline
static SgPipeOut g_pipe_out;
static SgParams  g_pipe_params;
static bool      g_pipe_enabled = true;
static bool      g_pipe_has_nvs = false;
static Preferences g_pipe_store;
static uint16_t  g_range_cm = 200;
static uint16_t clamp_range_cm(uint32_t cm);
static const uint32_t PIPE_CFG_VER = 1;

// HTTP/WS
static WebServer g_http_server(80);
static WiFiServer g_ws_server(81);
static SgHttpCtx g_http_ctx;
static char g_device_id[24] = {0};
static unsigned long g_ws_last_emit = 0;
static const unsigned long WS_PERIOD_MS = 500;

struct WsConn {
  WiFiClient client;
  bool active;
};
static WsConn g_ws_conns[4];

// MQTT
static const char* MQTT_HOST = "192.168.15.14"; // ajuste conforme broker
static const uint16_t MQTT_PORT = 1883;
static const char* MQTT_USER = "";
static const char* MQTT_PASS = "";
static const char* SB_REF   = "sb01";
static bool g_mqtt_enabled = true;
static bool g_mqtt_was_connected = false;

// range gate (2.00 m)
// timing
static const uint32_t TICK_PERIOD_MS = 250;
static const uint32_t STALE_MS       = 3000;

// warm-up pós config
static uint32_t g_after_cfg_ms = 0;

// ---------------------- Pipeline config persistente ----------------------
static SgParams pipe_default_params() {
  SgParams p;
  p.max_range_cm   = g_range_cm;
  p.hold_empty_ms  = 600;
  p.hold_exist_ms  = 250;
  p.hold_motion_ms = 150;
  p.snr_on_exist   = 0.10f;
  p.snr_off_exist  = 0.05f;
  p.snr_min        = 0.05f;
  p.snr_move       = 0.15f;
  p.delta_exist    = 0.05f;
  p.speed_thr_cms  = 5;
  p.k_ema          = 0.02f;
  return p;
}

static void pipe_save(const SgParams& p) {
  g_pipe_store.begin("pipe", false);
  // backup do valor anterior se existir versao
  if (g_pipe_store.isKey("ver")) {
    SgParams old;
    old.max_range_cm   = g_pipe_store.getUInt("dist_max", p.max_range_cm);
    old.hold_empty_ms  = g_pipe_store.getUInt("hold_e",  p.hold_empty_ms);
    old.hold_exist_ms  = g_pipe_store.getUInt("hold_p",  p.hold_exist_ms);
    old.hold_motion_ms = g_pipe_store.getUInt("hold_m",  p.hold_motion_ms);
    old.snr_on_exist   = g_pipe_store.getFloat("snr_on", p.snr_on_exist);
    old.snr_off_exist  = g_pipe_store.getFloat("snr_off",p.snr_off_exist);
    old.snr_min        = g_pipe_store.getFloat("snr_min",p.snr_min);
    old.snr_move       = g_pipe_store.getFloat("snr_mov",p.snr_move);
    old.delta_exist    = g_pipe_store.getFloat("delta_ex",p.delta_exist);
    old.speed_thr_cms  = g_pipe_store.getUInt("spd_thr", p.speed_thr_cms);
    old.k_ema          = g_pipe_store.getFloat("k_ema",  p.k_ema);
    g_pipe_store.putBool("b_enabled", g_pipe_store.getBool("enabled", g_pipe_enabled));
    g_pipe_store.putUInt("b_dist_max", old.max_range_cm);
    g_pipe_store.putUInt("b_hold_e",   old.hold_empty_ms);
    g_pipe_store.putUInt("b_hold_p",   old.hold_exist_ms);
    g_pipe_store.putUInt("b_hold_m",   old.hold_motion_ms);
    g_pipe_store.putFloat("b_snr_on",  old.snr_on_exist);
    g_pipe_store.putFloat("b_snr_off", old.snr_off_exist);
    g_pipe_store.putFloat("b_snr_min", old.snr_min);
    g_pipe_store.putFloat("b_snr_mov", old.snr_move);
    g_pipe_store.putFloat("b_delta_ex",old.delta_exist);
    g_pipe_store.putUInt("b_spd_thr",  old.speed_thr_cms);
    g_pipe_store.putFloat("b_k_ema",   old.k_ema);
  }
  g_pipe_store.putUInt("ver", PIPE_CFG_VER);
  g_pipe_store.putBool("enabled", g_pipe_enabled);
  g_pipe_store.putUInt("dist_max", p.max_range_cm);
  g_pipe_store.putUInt("hold_e",  p.hold_empty_ms);
  g_pipe_store.putUInt("hold_p",  p.hold_exist_ms);
  g_pipe_store.putUInt("hold_m",  p.hold_motion_ms);
  g_pipe_store.putFloat("snr_on", p.snr_on_exist);
  g_pipe_store.putFloat("snr_off",p.snr_off_exist);
  g_pipe_store.putFloat("snr_min",p.snr_min);
  g_pipe_store.putFloat("snr_mov",p.snr_move);
  g_pipe_store.putFloat("delta_ex",p.delta_exist);
  g_pipe_store.putUInt("spd_thr", p.speed_thr_cms);
  g_pipe_store.putFloat("k_ema",  p.k_ema);
  g_pipe_store.end();
}

static SgParams pipe_load_from_nvs(bool& enabled) {
  SgParams p = pipe_default_params();
  enabled = true;
  g_pipe_store.begin("pipe", true);
  uint32_t ver      = g_pipe_store.getUInt("ver", 0);
  g_pipe_has_nvs    = (ver == PIPE_CFG_VER) && g_pipe_store.isKey("dist_max");
  enabled          = g_pipe_store.getBool("enabled", enabled);
  p.max_range_cm   = clamp_range_cm(g_pipe_store.getUInt("dist_max", p.max_range_cm));
  p.hold_empty_ms  = g_pipe_store.getUInt("hold_e",  p.hold_empty_ms);
  p.hold_exist_ms  = g_pipe_store.getUInt("hold_p",  p.hold_exist_ms);
  p.hold_motion_ms = g_pipe_store.getUInt("hold_m",  p.hold_motion_ms);
  p.snr_on_exist   = g_pipe_store.getFloat("snr_on", p.snr_on_exist);
  p.snr_off_exist  = g_pipe_store.getFloat("snr_off",p.snr_off_exist);
  p.snr_min        = g_pipe_store.getFloat("snr_min",p.snr_min);
  p.snr_move       = g_pipe_store.getFloat("snr_mov",p.snr_move);
  p.delta_exist    = g_pipe_store.getFloat("delta_ex",p.delta_exist);
  p.speed_thr_cms  = g_pipe_store.getUInt("spd_thr", p.speed_thr_cms);
  p.k_ema          = g_pipe_store.getFloat("k_ema", p.k_ema);
  g_pipe_store.end();
  g_range_cm = clamp_range_cm(p.max_range_cm);
  p.max_range_cm = g_range_cm;
  return p;
}

static bool pipe_restore_backup(SgParams& out, bool& enabled) {
  g_pipe_store.begin("pipe", true);
  if (!g_pipe_store.isKey("b_dist_max")) { g_pipe_store.end(); return false; }
  enabled = g_pipe_store.getBool("b_enabled", enabled);
  out.max_range_cm   = clamp_range_cm(g_pipe_store.getUInt("b_dist_max", out.max_range_cm));
  out.hold_empty_ms  = g_pipe_store.getUInt("b_hold_e",  out.hold_empty_ms);
  out.hold_exist_ms  = g_pipe_store.getUInt("b_hold_p",  out.hold_exist_ms);
  out.hold_motion_ms = g_pipe_store.getUInt("b_hold_m",  out.hold_motion_ms);
  out.snr_on_exist   = g_pipe_store.getFloat("b_snr_on", out.snr_on_exist);
  out.snr_off_exist  = g_pipe_store.getFloat("b_snr_off",out.snr_off_exist);
  out.snr_min        = g_pipe_store.getFloat("b_snr_min",out.snr_min);
  out.snr_move       = g_pipe_store.getFloat("b_snr_mov",out.snr_move);
  out.delta_exist    = g_pipe_store.getFloat("b_delta_ex",out.delta_exist);
  out.speed_thr_cms  = g_pipe_store.getUInt("b_spd_thr", out.speed_thr_cms);
  out.k_ema          = g_pipe_store.getFloat("b_k_ema",  out.k_ema);
  g_pipe_store.end();
  return true;
}

static void pipe_clear_all() {
  g_pipe_store.begin("pipe", false);
  g_pipe_store.clear();
  g_pipe_store.end();
}

static void pipe_apply(const SgParams& p) {
  g_pipe_params = p;
  sg_pipe_set_params(p);
  pipe_save(p);
}

static float clamp01(float v) {
  if (v < 0.0f) return 0.0f;
  if (v > 1.0f) return 1.0f;
  return v;
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

static void uart_drain_rx(UartHandle* h, uint32_t max_ms=50) {
  if (!h || !h->impl) return;
  HardwareSerial* ser = reinterpret_cast<HardwareSerial*>(h->impl);
  uint32_t t0 = millis();
  while (millis() - t0 < max_ms) {
    while (ser->available() > 0) { ser->read(); }
    delay(2);
  }
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
      "\"state\":%d,\"stable\":%d,\"stable_ms\":%lu,\"in_range\":%s}\n",
      (unsigned long)millis(), status_str(p.status),
      dist_m, speed_ms, snr01,
      p.distance_cm, (int)p.speed_cms, p.signal,
      (int)o.state, (int)o.stable, (unsigned long)o.stable_ms,
      (p.distance_cm>0 && p.distance_cm<=g_range_cm)? "true":"false"
    );
  } else {
    Serial.printf("[PARSED] status=%s dist=%ucm signal=%u  state=%d stable=%d t=%lu\n",
                  status_str(p.status), p.distance_cm, p.signal,
                  (int)o.state, (int)o.stable, (unsigned long)millis());
  }
}

static void pipe_show(Print& out) {
  bool enabled = g_pipe_enabled;
  SgParams stored = pipe_load_from_nvs(enabled);
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

// ---------------------- Config do radar no boot ----------------------
// VO hold = 3000 ms (0x0BB8)
static const uint8_t SET_VO_HOLD_3S[]           = {0x55,0x5A,0x00,0x06,0x01,0x80,0x14,0x0B,0xB8,0x0D};
// Save all
static const uint8_t SAVE_ALL[]                 = {0x55,0x5A,0x00,0x04,0x01,0x20,0x04,0xD8};

static void radar_config_boot() {
  LOGI("[CFG] VO hold = 3000 ms");
  radar_send(SET_VO_HOLD_3S, sizeof(SET_VO_HOLD_3S));

  LOGI("[CFG] Presence max = %u cm", (unsigned)g_range_cm);
  radar_set_presence_max(g_range_cm);

  LOGI("[CFG] Save all");
  radar_send(SAVE_ALL, sizeof(SAVE_ALL));

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
}
static void cli_info(Print& out) {
  out.print(F("UART1 RX=")); out.print(SG_RADAR_RX);
  out.print(F(" TX=")); out.print(SG_RADAR_TX);
  out.print(F(" @")); out.println(SG_RADAR_BAUD);
  out.print(F("OCC pin=")); out.println(SG_PIN_RADAR_OCC);
  out.print(F("Range (max presence)=")); out.print(g_range_cm); out.println(F(" cm"));
}

static void on_cli_stream(bool enable) { g_stream = enable; LOGI("[CLI] stream %s", enable ? "on" : "off"); }
static void on_cli_rate(unsigned long hz) {
  if (hz == 0) g_stream_period_ms = 0;
  else        g_stream_period_ms = (unsigned long)max(1UL, 1000UL / hz);
  LOGI("[CLI] rate = %lu Hz (period %lu ms)", hz, g_stream_period_ms);
}
static void on_cli_json(bool enable) { g_stream_json = enable; LOGI("[CLI] json %s", enable ? "on" : "off"); }
static void on_cli_log(int lvl) { g_log_level = constrain(lvl, 0, 3); LOGI("[CLI] log level = %d", g_log_level); }

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

static uint16_t clamp_range_cm(uint32_t cm) {
  // Se veio em metros (ex.: 2,4,6) converte
  if (cm > 0 && cm <= 10) cm *= 100;
  // Três presets: 200/400/600 cm
  if (cm <= 250) return 200;
  if (cm <= 500) return 400;
  return 600;
}

static void on_cli_pipe(int argc, char* argv[], Print& out) {
  if (argc < 2) { out.println(F("[pipe] argumentos insuficientes")); return; }
  const char* sub = argv[1];

  if (!strcasecmp(sub, "on") || !strcasecmp(sub, "off")) {
    g_pipe_enabled = !strcasecmp(sub, "on");
    pipe_save(g_pipe_params);
    out.printf("[pipe] %s\n", g_pipe_enabled ? "on" : "off");
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

  if (!strcasecmp(key, "dist_max")) {
    uint32_t v=0; if (!parse_uint(val, v)) { out.println(F("[pipe] dist_max invalido")); return; }
    if (v > 0 && v <= 10) v *= 100; // aceita metros
    g_pipe_params.max_range_cm = clamp_range_cm(v);
    g_range_cm = g_pipe_params.max_range_cm;
    range_changed = true;
  } else if (!strcasecmp(key, "snr_min")) {
      float v=0; if (!parse_float(val, v)) { out.println(F("[pipe] snr_min invalido")); return; }
      g_pipe_params.snr_min = clamp01(v);
    } else if (!strcasecmp(key, "snr_move")) {
      float v=0; if (!parse_float(val, v)) { out.println(F("[pipe] snr_move invalido")); return; }
      g_pipe_params.snr_move = clamp01(v);
    } else if (!strcasecmp(key, "delta_exist")) {
      float v=0; if (!parse_float(val, v)) { out.println(F("[pipe] delta_exist invalido")); return; }
      g_pipe_params.delta_exist = constrain(v, 0.0f, 1.0f);
    } else if (!strcasecmp(key, "speed_thr")) {
      uint32_t v=0; if (!parse_uint(val, v)) { out.println(F("[pipe] speed_thr invalido")); return; }
      g_pipe_params.speed_thr_cms = (uint16_t)min<uint32_t>(v, 2000);
    } else if (!strcasecmp(key, "hold") && argc >= 5) {
      const char* which = argv[3];
      const char* vstr  = argv[4];
      uint32_t v=0; if (!parse_uint(vstr, v)) { out.println(F("[pipe] hold invalido")); return; }
      if (!strcasecmp(which, "presence")) g_pipe_params.hold_exist_ms  = (uint16_t)min<uint32_t>(v, 10000);
      else if (!strcasecmp(which, "motion")) g_pipe_params.hold_motion_ms = (uint16_t)min<uint32_t>(v, 10000);
      else if (!strcasecmp(which, "empty"))  g_pipe_params.hold_empty_ms  = (uint16_t)min<uint32_t>(v, 20000);
      else { out.println(F("[pipe] hold alvo invalido (presence|motion|empty)")); return; }
    } else if (!strcasecmp(key, "k_ema")) {
      float v=0; if (!parse_float(val, v)) { out.println(F("[pipe] k_ema invalido")); return; }
      g_pipe_params.k_ema = constrain(v, 0.001f, 0.2f);
    } else {
      changed = false;
    }

    if (!changed) { out.println(F("[pipe] chave desconhecida")); return; }
    pipe_apply(g_pipe_params);
    if (range_changed) {
      sg_pipe_reset_baseline();
      radar_set_presence_max(g_range_cm);
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
  SgCalibMetrics m = sg_calib_metrics(millis());
  SgCalibSuggest s = sg_calib_build_suggest(g_pipe_params);
  // opcional: log estruturado para calib_report.py consumir
  Serial.printf("{\"tag\":\"calib\",\"meta\":{\"ts_ms\":%lu,\"state\":\"%s\"},\"metrics\":{\"elapsed_ms\":%u,\"valid_ratio\":%.3f,\"snr_mean\":%.4f,\"snr_std\":%.4f,\"dist_p95_cm\":%u},\"current\":{\"max_range_cm\":%u,\"hold_empty_ms\":%u},\"suggest\":{\"max_range_cm\":%u,\"hold_empty_ms\":%u}}\n",
    (unsigned long)m.elapsed_ms,
    calib_state_str(sg_calib_state()),
    (unsigned)m.elapsed_ms,
    m.valid_ratio,
    m.snr_mean,
    m.snr_std,
    m.dist_p95_cm,
    g_pipe_params.max_range_cm,
    g_pipe_params.hold_empty_ms,
    s.max_range_cm,
    s.hold_empty_ms
  );
  out.print(F("{\"state\":\"")); out.print(calib_state_str(sg_calib_state())); out.print('"');
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

static bool calib_profile_name_ok(const char* name) {
  if (!name) return false;
  size_t n = strlen(name);
  if (n == 0 || n > 15) return false;
  for (size_t i = 0; i < n; ++i) {
    char c = name[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

static bool calib_profile_save(const char* name) {
  if (!calib_profile_name_ok(name)) return false;
  Preferences pref;
  pref.begin("calibpf", false);
  size_t n = pref.putBytes(name, &g_pipe_params, sizeof(SgParams));
  pref.end();
  return n == sizeof(SgParams);
}

static bool calib_profile_load(const char* name, SgParams& out) {
  if (!calib_profile_name_ok(name)) return false;
  Preferences pref;
  pref.begin("calibpf", true);
  size_t got = pref.getBytes(name, &out, sizeof(SgParams));
  pref.end();
  return got == sizeof(SgParams);
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
    bool ok = sg_calib_start(dur);
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
    SgCalibSuggest s = sg_calib_build_suggest(g_pipe_params);
    calib_preview(out, g_pipe_params, s);
    return;
  }

  if (!strcasecmp(sub, "abort")) {
    bool ok = sg_calib_abort();
    out.println(ok ? F("[calib] abortado") : F("[calib] nada para abortar"));
    return;
  }

  if (!strcasecmp(sub, "apply")) {
    if (sg_calib_state() != SG_CALIB_READY && sg_calib_state() != SG_CALIB_APPLIED) {
      out.println(F("[calib] nada para aplicar (run calib start e aguarde)"));
      return;
    }
    SgCalibSuggest sug = sg_calib_build_suggest(g_pipe_params);
    calib_preview(out, g_pipe_params, sug);
    uint16_t old_range = g_pipe_params.max_range_cm;
    bool ok = sg_calib_apply(&g_pipe_params);
    if (!ok) { out.println(F("[calib] apply falhou")); return; }
    sg_pipe_set_params(g_pipe_params);
    pipe_save(g_pipe_params);
    sg_pipe_reset_baseline();
    if (g_pipe_params.max_range_cm != old_range) {
      g_range_cm = g_pipe_params.max_range_cm;
      radar_set_presence_max(g_range_cm);
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
      if (!calib_profile_load(name, loaded)) { out.println(F("[calib] profile load falhou (nao existe?)")); return; }
      g_pipe_params = loaded;
      sg_pipe_set_params(g_pipe_params);
      pipe_save(g_pipe_params);
      sg_pipe_reset_baseline();
      g_range_cm = g_pipe_params.max_range_cm;
      radar_set_presence_max(g_range_cm);
      out.printf("[calib] profile '%s' aplicado e persistido\n", name);
      pipe_show(out);
      return;
    }
  }

  if (!strcasecmp(sub, "reset")) {
    sg_calib_reset();
    out.println(F("[calib] reset ok (coleta)"));
    return;
  }

  if (!strcasecmp(sub, "factory")) {
    pipe_clear_all();
    g_pipe_params = pipe_default_params();
    g_pipe_enabled = true;
    pipe_apply(g_pipe_params);
    sg_pipe_reset_baseline();
    radar_set_presence_max(g_pipe_params.max_range_cm);
    out.println(F("[calib] factory reset: params default aplicados e persistidos"));
    return;
  }

  if (!strcasecmp(sub, "rollback")) {
    SgParams bak = g_pipe_params;
    bool en = g_pipe_enabled;
    if (!pipe_restore_backup(bak, en)) {
      out.println(F("[calib] rollback indisponivel (sem backup)"));
      return;
    }
    g_pipe_params = bak;
    g_pipe_enabled = en;
    pipe_apply(g_pipe_params);
    sg_pipe_reset_baseline();
    radar_set_presence_max(g_pipe_params.max_range_cm);
    out.println(F("[calib] rollback aplicado a partir do backup"));
    pipe_show(out);
    return;
  }

  out.println(F("[calib] comando invalido"));
}

static void radar_set_presence_max(uint16_t cm) {
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
  uint8_t sum = (uint8_t)((frame[4] + frame[5] + frame[6] + frame[7] + frame[8]) & 0xFF); // checksum legacy do vendor
  frame[9] = sum;
  LOGI("[CFG] Presence max = %u cm (sum=0x%02X)", (unsigned)cm, (unsigned)sum);
  radar_send(frame, sizeof(frame));
}

static void on_cli_range(uint32_t cm) {
  if (cm > 0 && cm <= 10) cm *= 100; // aceita metros (2/4/6)
  uint16_t target = clamp_range_cm(cm);
  g_range_cm = target;
  g_pipe_params.max_range_cm = target;
  pipe_apply(g_pipe_params);
  sg_pipe_reset_baseline();
  radar_set_presence_max(target);
  LOGI("[CLI] range set to %u cm (preset)", (unsigned)target);
}

// ---------------------- HTTP/WS helpers ----------------------
static void make_device_id() {
  uint64_t mac = ESP.getEfuseMac();
  snprintf(g_device_id, sizeof(g_device_id), "sg-%04X%08X",
           (uint16_t)(mac >> 32), (uint32_t)mac);
}

static bool ws_handshake(WiFiClient& c) {
  const char* magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  char buf[512];
  int n = c.readBytesUntil('\n', buf, sizeof(buf)-1); // first line
  if (n <= 0) return false;
  // read headers
  String key;
  while (c.connected()) {
    String line = c.readStringUntil('\n');
    if (line.length() == 0 || line == "\r") break;
    if (line.startsWith("Sec-WebSocket-Key")) {
      int p = line.indexOf(':');
      if (p >= 0) {
        key = line.substring(p+1);
        key.trim();
      }
    }
  }
  if (key.length() == 0) return false;
  String accept_src = key + magic;
  unsigned char sha_out[20];
  mbedtls_sha1_context sha;
  mbedtls_sha1_init(&sha);
  mbedtls_sha1_starts(&sha);
  mbedtls_sha1_update(&sha, (const unsigned char*)accept_src.c_str(), accept_src.length());
  mbedtls_sha1_finish(&sha, sha_out);
  mbedtls_sha1_free(&sha);
  unsigned char b64[64];
  size_t b64_len = 0;
  mbedtls_base64_encode(b64, sizeof(b64), &b64_len, sha_out, 20);
  String accept = String((const char*)b64).substring(0, b64_len);
  c.printf("HTTP/1.1 101 Switching Protocols\r\n");
  c.printf("Upgrade: websocket\r\n");
  c.printf("Connection: Upgrade\r\n");
  c.printf("Sec-WebSocket-Accept: %s\r\n\r\n", accept.c_str());
  return true;
}

static void ws_broadcast(const char* msg) {
  size_t len = strlen(msg);
  uint8_t hdr[10];
  size_t hlen = 0;
  hdr[0] = 0x81; // FIN + text
  if (len < 126) {
    hdr[1] = (uint8_t)len;
    hlen = 2;
  } else if (len < 65536) {
    hdr[1] = 126;
    hdr[2] = (len >> 8) & 0xFF;
    hdr[3] = len & 0xFF;
    hlen = 4;
  } else {
    return; // too big
  }
  for (int i = 0; i < (int)(sizeof(g_ws_conns)/sizeof(g_ws_conns[0])); ++i) {
    if (!g_ws_conns[i].active) continue;
    if (!g_ws_conns[i].client.connected()) { g_ws_conns[i].active=false; continue; }
    g_ws_conns[i].client.write(hdr, hlen);
    g_ws_conns[i].client.write((const uint8_t*)msg, len);
  }
}

static void ws_accept_clients() {
  WiFiClient c = g_ws_server.available();
  if (!c) return;
  if (!ws_handshake(c)) { c.stop(); return; }
  for (int i = 0; i < (int)(sizeof(g_ws_conns)/sizeof(g_ws_conns[0])); ++i) {
    if (!g_ws_conns[i].active) {
      g_ws_conns[i].client = c;
      g_ws_conns[i].active = true;
      return;
    }
  }
  c.stop(); // no slot
}

// ---------------------- MQTT helpers ----------------------
static void mqtt_build_meas(char* out, size_t out_sz) {
  if (!out || out_sz==0 || !g_has_last) { if(out_sz>0) out[0]=0; return; }
  char payload[256];
  snprintf(payload, sizeof(payload),
    "{\"measures\":[{\"sensor\":\"radar\",\"qty\":\"distance\",\"value\":%.2f,\"unit\":\"m\"},"
    "{\"sensor\":\"radar\",\"qty\":\"speed\",\"value\":%.2f,\"unit\":\"m/s\"},"
    "{\"sensor\":\"radar\",\"qty\":\"signal\",\"value\":%u,\"unit\":\"au\"}],"
    "\"status\":%d}",
    g_last.distance_cm*0.01f,
    g_last.speed_cms*0.01f,
    g_last.signal,
    (int)g_pipe_out.stable);
  sg_http_next_seq(&g_http_ctx);
  sg_http_envelope(&g_http_ctx, g_last_seen_ms, "meas", payload, out, out_sz);
}

static void mqtt_build_status(char* out, size_t out_sz) {
  char payload[128];
  snprintf(payload, sizeof(payload),
    "{\"fw\":\"1.0.0\",\"uptime_s\":%lu,\"rssi_dbm\":0}",
    (unsigned long)(millis()/1000));
  sg_http_next_seq(&g_http_ctx);
  sg_http_envelope(&g_http_ctx, millis(), "status", payload, out, out_sz);
}

static void mqtt_build_cap(char* out, size_t out_sz) {
  const char* payload = "{\"sensors\":[{\"name\":\"radar\",\"measures\":[\"distance:m\",\"speed:m/s\",\"signal:au\"],\"events\":[\"presence\"]}]}";
  sg_http_next_seq(&g_http_ctx);
  sg_http_envelope(&g_http_ctx, millis(), "cap", payload, out, out_sz);
}

static void mqtt_build_ack(const char* txid, int ok, char* out, size_t out_sz) {
  sg_http_next_seq(&g_http_ctx);
  sg_http_make_ack(&g_http_ctx, millis(), txid, ok, out, out_sz);
}

static void mqtt_build_err(const char* txid, const char* code, const char* msg, char* out, size_t out_sz) {
  sg_http_next_seq(&g_http_ctx);
  sg_http_make_err(&g_http_ctx, millis(), txid, code, msg, out, out_sz);
}

// Parsers simples (sem JSON lib) para cmd MQTT
static bool json_get_str_key(const char* payload, size_t len, const char* key_with_quotes, char* out, size_t out_sz) {
  if (!payload || !key_with_quotes || !out || out_sz==0) return false;
  out[0]=0;
  const char* p = strstr(payload, key_with_quotes);
  if (!p) return false;
  p += strlen(key_with_quotes);
  const char* start = p;
  while (*p && *p!='\"' && (size_t)(p-payload)<len) p++;
  size_t n = (size_t)(p-start);
  if (n >= out_sz) n = out_sz-1;
  memcpy(out, start, n); out[n]=0;
  return true;
}

static bool json_get_int_key(const char* payload, size_t len, const char* key, uint32_t& out) {
  if (!payload || !key) return false;
  const char* p = strstr(payload, key);
  if (!p) return false;
  p = strchr(p, ':'); if (!p) return false;
  p++;
  out = strtoul(p, NULL, 10);
  return true;
}
static void http_send_json(const char* json) {
  g_http_server.send(200, "application/json", json);
}

static void handle_http_occupancy() {
  sg_http_next_seq(&g_http_ctx);
  SgHttpOccupancy occ;
  occ.ts_ms = millis();
  occ.state = (int)g_pipe_out.stable;
  occ.confidence = (g_pipe_out.stable == SG_EMPTY) ? 0.1f : 0.9f;
  char buf[256];
  sg_http_make_occupancy(&g_http_ctx, &occ, buf, sizeof(buf));
  http_send_json(buf);
}

static void handle_http_tracks() {
  sg_http_next_seq(&g_http_ctx);
  SgHttpTracks tr;
  tr.ts_ms = millis();
  tr.count_active = 0; // placeholder
  char buf[192];
  sg_http_make_tracks(&g_http_ctx, &tr, buf, sizeof(buf));
  http_send_json(buf);
}

static void handle_http_health() {
  sg_http_next_seq(&g_http_ctx);
  SgHttpHealth h;
  h.ts_ms = millis();
  h.uptime_s = (uint32_t)(millis() / 1000);
  h.rssi_dbm = 0;
  char buf[192];
  sg_http_make_health(&g_http_ctx, &h, buf, sizeof(buf));
  http_send_json(buf);
}

static void handle_http_meas() {
  if (!g_has_last) { g_http_server.send(404, "application/json", "{\"error\":\"no_data\"}"); return; }
  char buf[256];
  int n = snprintf(buf, sizeof(buf),
    "{\"ts_ms\":%lu,\"status\":\"%s\",\"dist_m\":%.3f,\"speed_mps\":%.3f,"
    "\"snr\":%.3f,\"distance_cm\":%u,\"speed_cms\":%d,\"signal\":%u,"
    "\"state\":%d,\"stable\":%d,\"stable_ms\":%lu,\"in_range\":%s}",
    (unsigned long)g_last_seen_ms,
    status_str(g_last.status),
    g_last.distance_cm * 0.01f,
    g_last.speed_cms * 0.01f,
    g_last.snr,
    g_last.distance_cm,
    (int)g_last.speed_cms,
    g_last.signal,
    (int)g_pipe_out.state,
    (int)g_pipe_out.stable,
    (unsigned long)g_pipe_out.stable_ms,
    (g_last.distance_cm>0 && g_last.distance_cm<=g_range_cm)? "true":"false"
  );
  (void)n;
  g_http_server.send(200, "application/json", buf);
}

static String read_body() {
  if (!g_http_server.hasArg("plain")) return String();
  return g_http_server.arg("plain");
}

static void handle_http_cmd() {
  String body = read_body();
  char txid_buf[32] = {0};
  const char* txid = "";
  // parse txid basico (nao robusto)
  int idx = body.indexOf("\"txid\"");
  if (idx >= 0) {
    int q1 = body.indexOf('"', idx + 6);
    int q2 = body.indexOf('"', q1 + 1);
    if (q1 >= 0 && q2 > q1) {
      body.substring(q1 + 1, q2).toCharArray(txid_buf, sizeof(txid_buf));
      txid = txid_buf;
    }
  }
  sg_http_next_seq(&g_http_ctx);
  char buf[192];
  sg_http_make_ack(&g_http_ctx, millis(), txid, 1, buf, sizeof(buf));
  http_send_json(buf);
}

static void setup_http_ws() {
  make_device_id();
  sg_http_init(&g_http_ctx, g_device_id, 1);
  g_http_server.on("/v1/occupancy", HTTP_GET, handle_http_occupancy);
  g_http_server.on("/v1/tracks", HTTP_GET, handle_http_tracks);
  g_http_server.on("/v1/health", HTTP_GET, handle_http_health);
  g_http_server.on("/v1/meas", HTTP_GET, handle_http_meas);
  g_http_server.on("/v1/cmd", HTTP_POST, handle_http_cmd);
  g_http_server.begin();
  g_ws_server.begin();
  for (int i = 0; i < (int)(sizeof(g_ws_conns)/sizeof(g_ws_conns[0])); ++i) {
    g_ws_conns[i].active = false;
  }
}

static void on_mqtt_cmd(const char* payload, size_t len) {
  if (!payload) return;
  char txid[32]; json_get_str_key(payload, len, "\"txid\":\"", txid, sizeof(txid));
  char op[32] = {0};
  json_get_str_key(payload, len, "\"op\":\"", op, sizeof(op));
  LOGI("[MQTT cmd] op=%s txid=%s", op, txid);
  char ackbuf[256], errbuf[256];
  if (!strcasecmp(op, "calib.start")) {
    uint32_t dur = 60000;
    json_get_int_key(payload, len, "dur_ms", dur);
    if (dur == 0) dur = 60000;
    bool ok = sg_calib_start(dur);
    if (ok) {
      mqtt_build_ack(txid, 1, ackbuf, sizeof(ackbuf));
      sg_mqtt_pub_ack(ackbuf);
    } else {
      mqtt_build_err(txid, "busy", "calib in progress", errbuf, sizeof(errbuf));
      sg_mqtt_pub_err(errbuf);
    }
    return;
  }
  if (!strcasecmp(op, "set")) {
    char path[64];
    if (!json_get_str_key(payload, len, "\"path\":\"", path, sizeof(path))) {
      mqtt_build_err(txid, "bad_path", "path missing", errbuf, sizeof(errbuf));
      sg_mqtt_pub_err(errbuf);
      return;
    }
    if (!strcmp(path, "pipe.dist_max")) {
      uint32_t v = 0; json_get_int_key(payload, len, "value", v);
      if (v > 0 && v <= 10) v *= 100;
      g_pipe_params.max_range_cm = clamp_range_cm(v);
      g_range_cm = g_pipe_params.max_range_cm;
      pipe_apply(g_pipe_params);
      sg_pipe_reset_baseline();
      radar_set_presence_max(g_range_cm);
      mqtt_build_ack(txid, 1, ackbuf, sizeof(ackbuf));
      sg_mqtt_pub_ack(ackbuf);
      return;
    }
    mqtt_build_err(txid, "unknown_path", path, errbuf, sizeof(errbuf));
    sg_mqtt_pub_err(errbuf);
    return;
  }
  mqtt_build_err(txid, "unknown_op", op, errbuf, sizeof(errbuf));
  sg_mqtt_pub_err(errbuf);
}

// ---------------------- Setup/Loop ----------------------
void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 1500) { /* aguarda enumerar */ }

  // AP fechado para teste HTTP/WS
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("SenseGrid", "esp929305");
  IPAddress apIP = WiFi.softAPIP();
  LOGI("[NET] AP SenseGrid iniciado em %s", apIP.toString().c_str());

  // STA na rede local
  WiFi.begin("PIZZIOLO_2G", "revil2301revil2301");
  uint32_t t_sta = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t_sta) < 10000) {
    delay(200);
  }
  if (WiFi.status() == WL_CONNECTED) {
    LOGI("[NET] STA conectado em %s IP=%s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  } else {
    LOGW("[NET] STA nao conectou; mantendo apenas AP");
  }

  pinMode(SG_PIN_RADAR_OCC, INPUT_PULLDOWN);

  if (!uart_begin(&g_uart, /*uart*/1, SG_RADAR_BAUD, /*RX*/SG_RADAR_RX, /*TX*/SG_RADAR_TX)) {
    LOGE("[ERR] uart_begin failed");
  }
  if (!radar_begin(&g_radar, &g_uart)) {
    LOGE("[ERR] radar_begin failed");
  }

  ring_init(&g_ring, g_samples, 256);
  sg_pipe_init(); // pipeline default
  g_pipe_params = pipe_load_from_nvs(g_pipe_enabled);
  sg_pipe_set_params(g_pipe_params);
  if (!g_pipe_has_nvs) {
    pipe_save(g_pipe_params); // garante que pipe show leia o que está em NVS
  }
  g_range_cm = g_pipe_params.max_range_cm;

  LOGI("[CFG] stream default=on (use 'stream off' para silenciar)");

  Serial.println();
  LOGI("[BOOT] SenseGrid + Pipeline + CLI");
  Serial.printf("[UART1] RX=%d TX=%d @%lu\n", SG_RADAR_RX, SG_RADAR_TX, (unsigned long)SG_RADAR_BAUD);
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
    /*calib */ on_cli_calib
  );

  setup_http_ws();

  if (g_mqtt_enabled) {
    SgMqttCfg cfg;
    cfg.host = MQTT_HOST;
    cfg.port = MQTT_PORT;
    cfg.user = MQTT_USER;
    cfg.pass = MQTT_PASS;
    cfg.sb_ref = SB_REF;
    cfg.device_id = g_device_id;
    cfg.keepalive_s = 30;
    sg_mqtt_init(&cfg, on_mqtt_cmd);
  }
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
    for (int i = 0; i < 3; ++i) {
      RadarParsed p;
      if (!radar_read_parsed(&g_radar, &p, /*timeout_ms*/20)) break;

      g_last = p;
      g_has_last = true;
      g_last_seen_ms = millis();

      // pipeline (básico)
      SgPipeIn in { p.status, p.distance_cm, p.speed_cms, p.snr, g_last_seen_ms };
      bool in_range = (p.distance_cm > 0 && p.distance_cm <= g_range_cm);

      // calibração: coleta baseline vazio (ou o que estiver configurado)
      sg_calib_push_sample(in);

      if (g_pipe_enabled) {
        g_pipe_out = sg_pipe_step(in);
      } else {
        // bypass simples quando pipeline desligado
        SgState passthrough = in_range
                              ? (in.raw_status == 1 ? SG_MOTION :
                                 in.raw_status == 2 ? SG_PRESENCE : SG_EMPTY)
                              : SG_EMPTY;
        g_pipe_out.state     = passthrough;
        g_pipe_out.stable    = passthrough;
        g_pipe_out.stable_ms = 0;
        g_pipe_out.gated     = in_range;
      }

      // Se fora do range, zera e não emite
      if (!in_range) {
        g_pipe_out.state  = SG_EMPTY;
        g_pipe_out.stable = SG_EMPTY;
        g_pipe_out.gated  = false;
      }

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
      if (g_stream && in_range) {
        // respeita período se configurado
        static unsigned long last_emit = 0;
        unsigned long now = millis();
        if (g_stream_period_ms == 0 || (now - last_emit) >= g_stream_period_ms) {
          print_json(p, g_pipe_out);
          last_emit = now;
        }
      }
      if (g_mqtt_enabled && sg_mqtt_connected()) {
        char env[384];
        mqtt_build_meas(env, sizeof(env));
        sg_mqtt_pub_meas(env);
      }
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
  g_http_server.handleClient();
  ws_accept_clients();
  unsigned long now = millis();
  if (now - g_ws_last_emit >= WS_PERIOD_MS) {
    g_ws_last_emit = now;
    sg_http_next_seq(&g_http_ctx);
    SgHttpOccupancy occ;
    occ.ts_ms = now;
    occ.state = (int)g_pipe_out.stable;
    occ.confidence = (g_pipe_out.stable == SG_EMPTY) ? 0.1f : 0.9f;
    char buf[256];
    sg_http_make_occupancy(&g_http_ctx, &occ, buf, sizeof(buf));
    ws_broadcast(buf);
  }

  // 6) MQTT
  if (g_mqtt_enabled) {
    sg_mqtt_loop();
    bool c = sg_mqtt_connected();
    if (c && !g_mqtt_was_connected) {
      char env[384];
      mqtt_build_cap(env, sizeof(env)); sg_mqtt_pub_cap(env);
      mqtt_build_status(env, sizeof(env)); sg_mqtt_pub_status(env);
    }
    g_mqtt_was_connected = c;
  }

  delay(2);
}
