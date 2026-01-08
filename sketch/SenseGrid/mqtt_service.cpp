#include "mqtt_service.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "../../components/adapters_mqtt/include/adapters_mqtt.h"
#include "../../components/adapters_http/include/adapters_http.h"
#include "mqtt_config.h"

static char g_mqtt_host[64] = "192.168.15.9"; // ajuste conforme broker
static uint16_t g_mqtt_port = 1883;
static const char* MQTT_USER = "";
static const char* MQTT_PASS = "";
static const char* SB_REF   = "sb01";
static bool g_mqtt_enabled = true;
static bool g_mqtt_was_connected = false;
static int g_last_event_state = -1;
static MqttRuntimeCfg g_mqtt_cfg{};
static SgTelemetryCtx* g_tctx = nullptr;
static MqttApplyRangeFn g_apply_range_cb = nullptr;

static void mqtt_save_cfg() {
  MqttRuntimeCfg cfg{};
  strlcpy(cfg.host, g_mqtt_host, sizeof(cfg.host));
  cfg.port = g_mqtt_port;
  mqtt_config_save(&cfg);
}

static void mqtt_load_cfg() {
  MqttRuntimeCfg def{};
  strlcpy(def.host, g_mqtt_host, sizeof(def.host));
  def.port = g_mqtt_port;
  mqtt_config_load(&g_mqtt_cfg, &def);
  strlcpy(g_mqtt_host, g_mqtt_cfg.host, sizeof(g_mqtt_host));
  g_mqtt_port = g_mqtt_cfg.port;
}

static void mqtt_build_meas(char* out, size_t out_sz) {
  const SgCoreSnapshot* snap = sg_core_get_snapshot();
  if (!out || out_sz==0 || !snap || !snap->has_meas) { if(out_sz>0) out[0]=0; return; }
  char payload[256];
  snprintf(payload, sizeof(payload),
    "{\"measures\":[{\"sensor\":\"radar\",\"qty\":\"distance\",\"value\":%.2f,\"unit\":\"m\"},"
    "{\"sensor\":\"radar\",\"qty\":\"speed\",\"value\":%.2f,\"unit\":\"m/s\"},"
    "{\"sensor\":\"radar\",\"qty\":\"signal\",\"value\":%u,\"unit\":\"au\"}],"
    "\"status\":%d}",
    snap->meas.distance_cm*0.01f,
    snap->meas.speed_cms*0.01f,
    snap->meas.signal,
    (int)snap->pipe.stable);
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) { if (out_sz > 0) out[0]=0; return; }
  sg_http_next_seq(ctx);
  sg_http_envelope(ctx, snap->meas_ms, "meas", payload, out, out_sz);
}

static void mqtt_build_meas_raw(char* out, size_t out_sz) {
  const SgCoreSnapshot* snap = sg_core_get_snapshot();
  if (!out || out_sz==0 || !snap || !snap->has_meas) { if(out_sz>0) out[0]=0; return; }
  snprintf(out, out_sz,
    "{\"ts_ms\":%lu,\"status\":\"%s\",\"dist_m\":%.3f,\"speed_mps\":%.3f,"
    "\"snr\":%.3f,\"distance_cm\":%u,\"speed_cms\":%d,\"signal\":%u,"
    "\"az_deg\":%d,\"el_deg\":%d,"
    "\"state\":%d,\"stable\":%d,\"stable_ms\":%lu,\"in_range\":%s}",
    (unsigned long)snap->meas_ms,
    (snap->meas.status == 0 ? "none" : snap->meas.status == 1 ? "move" : snap->meas.status == 2 ? "exist" : "?"),
    snap->meas.distance_cm * 0.01f,
    snap->meas.speed_cms * 0.01f,
    snap->meas.snr,
    snap->meas.distance_cm,
    (int)snap->meas.speed_cms,
    snap->meas.signal,
    (int)snap->meas.azim_deg,
    (int)snap->meas.elev_deg,
    (int)snap->pipe.state,
    (int)snap->pipe.stable,
    (unsigned long)snap->pipe.stable_ms,
    snap->in_range ? "true" : "false"
  );
}

static void mqtt_build_status(char* out, size_t out_sz) {
  char payload[128];
  snprintf(payload, sizeof(payload),
    "{\"fw\":\"1.0.0\",\"uptime_s\":%lu,\"rssi_dbm\":0}",
    (unsigned long)(millis()/1000));
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) { if (out_sz > 0) out[0]=0; return; }
  sg_http_next_seq(ctx);
  sg_http_envelope(ctx, millis(), "status", payload, out, out_sz);
}

static void mqtt_build_dt_meta(char* out, size_t out_sz) {
  char payload[160];
  snprintf(payload, sizeof(payload),
    "{\"device_id\":\"%s\",\"device_model\":\"ESP32-C3\",\"provisioned_at\":0,\"version\":\"1.0.0\",\"last_update\":0,\"ota_enabled\":false}",
    sg_telemetry_device_id(g_tctx));
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) { if (out_sz > 0) out[0]=0; return; }
  sg_http_next_seq(ctx);
  sg_http_envelope(ctx, millis(), "meta", payload, out, out_sz);
}

static void mqtt_build_dt_status(char* out, size_t out_sz) {
  char payload[96];
  snprintf(payload, sizeof(payload),
    "{\"device_online\":true,\"last_time_online\":%lu}",
    (unsigned long)(millis()/1000));
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) { if (out_sz > 0) out[0]=0; return; }
  sg_http_next_seq(ctx);
  sg_http_envelope(ctx, millis(), "st", payload, out, out_sz);
}

static void mqtt_build_dt_ota(char* out, size_t out_sz) {
  const char* payload = "{\"ota_status\":\"disabled\",\"last_check\":0}";
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) { if (out_sz > 0) out[0]=0; return; }
  sg_http_next_seq(ctx);
  sg_http_envelope(ctx, millis(), "ota", payload, out, out_sz);
}

static void mqtt_build_cfg_out(char* out, size_t out_sz) {
  const char* payload =
    "{\"meta\":{\"hw_label\":\"Saida 1\",\"type\":\"0x01\"},"
    "\"settings\":{\"mode\":\"0x01\",\"control_value\":\"0x00\",\"pulse_time\":1000,\"boot_behavior\":\"off\"},"
    "\"digital_child\":{\"digital_device_id\":\"none\",\"output_function\":\"0x01\"}}";
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) { if (out_sz > 0) out[0]=0; return; }
  sg_http_next_seq(ctx);
  sg_http_envelope(ctx, millis(), "cfg_out", payload, out, out_sz);
}

static void mqtt_build_cfg_in(char* out, size_t out_sz) {
  const char* payload =
    "{\"meta\":{\"hw_label\":\"Entrada 1\",\"type\":\"0x01\"},"
    "\"settings\":{\"mode\":\"0x01\"},"
    "\"targets\":{\"target_1\":{\"type\":\"0x10\",\"destination\":\"self:output_1\"}}}";
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) { if (out_sz > 0) out[0]=0; return; }
  sg_http_next_seq(ctx);
  sg_http_envelope(ctx, millis(), "cfg_in", payload, out, out_sz);
}

static void mqtt_build_out_state(char* out, size_t out_sz) {
  const char* payload = "{\"state\":false}";
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) { if (out_sz > 0) out[0]=0; return; }
  sg_http_next_seq(ctx);
  sg_http_envelope(ctx, millis(), "o/out/output_1", payload, out, out_sz);
}

static void mqtt_build_in_state(char* out, size_t out_sz) {
  const char* payload = "{\"state\":false}";
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) { if (out_sz > 0) out[0]=0; return; }
  sg_http_next_seq(ctx);
  sg_http_envelope(ctx, millis(), "o/in/input_1", payload, out, out_sz);
}

static void mqtt_build_cap(char* out, size_t out_sz) {
  const char* payload = "{\"sensors\":[{\"name\":\"radar\",\"measures\":[\"distance:m\",\"speed:m/s\",\"signal:au\"],\"events\":[\"presence\"]}]}";
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) { if (out_sz > 0) out[0]=0; return; }
  sg_http_next_seq(ctx);
  sg_http_envelope(ctx, millis(), "cap", payload, out, out_sz);
}

static void mqtt_build_ack(const char* txid, int ok, char* out, size_t out_sz) {
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) { if (out_sz > 0) out[0]=0; return; }
  sg_http_next_seq(ctx);
  sg_http_make_ack(ctx, millis(), txid, ok, out, out_sz);
}

static void mqtt_build_err(const char* txid, const char* code, const char* msg, char* out, size_t out_sz) {
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) { if (out_sz > 0) out[0]=0; return; }
  sg_http_next_seq(ctx);
  sg_http_make_err(ctx, millis(), txid, code, msg, out, out_sz);
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

static void on_mqtt_cmd(const char* payload, size_t len) {
  if (!payload) return;
  char txid[32]; json_get_str_key(payload, len, "\"txid\":\"", txid, sizeof(txid));
  char op[32] = {0};
  json_get_str_key(payload, len, "\"op\":\"", op, sizeof(op));
  Serial.printf("[MQTT cmd] op=%s txid=%s\n", op, txid);
  char ackbuf[256], errbuf[256];
  if (!strcasecmp(op, "calib.start")) {
    uint32_t dur = 60000;
    json_get_int_key(payload, len, "dur_ms", dur);
    if (dur == 0) dur = 60000;
    bool ok = sg_core_calib_start(dur);
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
      sg_core_set_range_cm((uint16_t)v, true);
      sg_core_reset_baseline();
      if (g_apply_range_cb) g_apply_range_cb(sg_core_get_range_cm());
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

static bool parse_uint(const char* s, uint32_t& out) {
  if (!s) return false;
  char* end=nullptr;
  unsigned long v = strtoul(s, &end, 10);
  if (!(end && *end == 0)) return false;
  out = (uint32_t)v;
  return true;
}

void mqtt_service_init(SgTelemetryCtx* tctx, MqttApplyRangeFn apply_range_cb) {
  g_tctx = tctx;
  g_apply_range_cb = apply_range_cb;
  mqtt_load_cfg();
  if (g_mqtt_enabled) {
    SgMqttCfg cfg;
    cfg.host = g_mqtt_host;
    cfg.port = g_mqtt_port;
    cfg.user = MQTT_USER;
    cfg.pass = MQTT_PASS;
    cfg.sb_ref = SB_REF;
    cfg.device_id = sg_telemetry_device_id(g_tctx);
    cfg.keepalive_s = 30;
    sg_mqtt_init(&cfg, on_mqtt_cmd);
    g_mqtt_was_connected = false;
  }
}

void mqtt_service_loop() {
  if (!g_mqtt_enabled) return;
  sg_mqtt_loop();
  bool c = sg_mqtt_connected();
  if (c && !g_mqtt_was_connected) {
    char env[512];
    mqtt_build_cap(env, sizeof(env)); sg_mqtt_pub_cap(env);
    mqtt_build_status(env, sizeof(env)); sg_mqtt_pub_status(env);
    mqtt_build_dt_meta(env, sizeof(env)); sg_mqtt_publish("dt/meta", env, true, 1);
    mqtt_build_dt_status(env, sizeof(env)); sg_mqtt_publish("dt/st", env, true, 1);
    mqtt_build_dt_ota(env, sizeof(env)); sg_mqtt_publish("dt/ota", env, true, 1);
    mqtt_build_cfg_out(env, sizeof(env)); sg_mqtt_publish("dt/cfg/out/output_1", env, true, 1);
    mqtt_build_cfg_in(env, sizeof(env)); sg_mqtt_publish("dt/cfg/in/input_1", env, true, 1);
    mqtt_build_out_state(env, sizeof(env)); sg_mqtt_publish("dt/o/out/output_1", env, false, 0);
    mqtt_build_in_state(env, sizeof(env)); sg_mqtt_publish("dt/o/in/input_1", env, false, 0);
  }
  g_mqtt_was_connected = c;
}

void mqtt_service_on_measurement(const SgCoreSnapshot* snap) {
  if (!g_mqtt_enabled || !snap || !snap->has_meas) return;
  if (!sg_mqtt_connected()) return;
  char env[384];
  mqtt_build_meas(env, sizeof(env));
  sg_mqtt_pub_meas(env);
  char raw[384];
  mqtt_build_meas_raw(raw, sizeof(raw));
  if (raw[0]) sg_mqtt_pub_meas_raw(raw);
  // publish event em transicao de estado
  if (snap->pipe.stable != g_last_event_state) {
    g_last_event_state = snap->pipe.stable;
    const char* st = (g_last_event_state==0)?"empty":(g_last_event_state==1)?"presence":"motion";
    char ev[256];
    char payload[96];
    snprintf(payload, sizeof(payload), "{\"class\":\"presence.changed\",\"state\":\"%s\"}", st);
    SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
    if (ctx) {
      sg_http_next_seq(ctx);
      sg_http_envelope(ctx, snap->meas_ms, "event", payload, ev, sizeof(ev));
      sg_mqtt_pub_event(ev);
    }
  }
}

void mqtt_service_cli(int argc, char* argv[], Print& out) {
  if (argc < 2) {
    out.println(F("[mqtt] uso: mqtt show | host <addr> | port <num> | restart"));
    return;
  }
  const char* sub = argv[1];
  if (!strcasecmp(sub, "show")) {
    out.printf("[mqtt] host=%s port=%u\n", g_mqtt_host, (unsigned)g_mqtt_port);
    return;
  }
  if (!strcasecmp(sub, "host") && argc >= 3) {
    strlcpy(g_mqtt_host, argv[2], sizeof(g_mqtt_host));
    mqtt_save_cfg();
    out.printf("[mqtt] host salvo: %s\n", g_mqtt_host);
    return;
  }
  if (!strcasecmp(sub, "port") && argc >= 3) {
    uint32_t p=0; if (!parse_uint(argv[2], p)) { out.println(F("[mqtt] porta invalida")); return; }
    g_mqtt_port = (uint16_t)p;
    mqtt_save_cfg();
    out.printf("[mqtt] port salvo: %u\n", (unsigned)g_mqtt_port);
    return;
  }
  if (!strcasecmp(sub, "restart")) {
    mqtt_save_cfg();
    if (g_mqtt_enabled) {
      SgMqttCfg cfg;
      cfg.host = g_mqtt_host;
      cfg.port = g_mqtt_port;
      cfg.user = MQTT_USER;
      cfg.pass = MQTT_PASS;
      cfg.sb_ref = SB_REF;
      cfg.device_id = sg_telemetry_device_id(g_tctx);
      cfg.keepalive_s = 30;
      sg_mqtt_init(&cfg, on_mqtt_cmd);
      g_mqtt_was_connected = false;
    }
    out.println(F("[mqtt] restart solicitado"));
    return;
  }
  out.println(F("[mqtt] comando invalido"));
}
