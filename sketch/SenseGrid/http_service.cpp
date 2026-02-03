#include "http_service.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha1.h>
#include <string.h>
#include <ctype.h>
#include "net_service.h"
#include "sg_adapters_logger.h"
#include "http_monitor_page.h"

static WebServer g_http_server(80);
static WiFiServer g_ws_server(81);
static SgTelemetryCtx* g_tctx = nullptr;
static SgNetInfo* g_net_info = nullptr;
static unsigned long g_ws_last_emit = 0;
static const unsigned long WS_PERIOD_MS = 500;

struct WsConn {
  WiFiClient client;
  bool active;
};
static WsConn g_ws_conns[4];

static const char* status_str(uint8_t s) {
  switch (s) {
    case 0: return "none";
    case 1: return "move";
    case 2: return "exist";
    default: return "?";
  }
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

static String ip_to_str(const IPAddress& ip) {
  return ip.toString();
}

static void http_set_cors() {
  g_http_server.sendHeader("Access-Control-Allow-Origin", "*");
  g_http_server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
  g_http_server.sendHeader("Access-Control-Allow-Headers", "*");
}

static void http_send_json(const char* json) {
  http_set_cors();
  g_http_server.send(200, "application/json", json);
}

static bool parse_bool_str(const String& v) {
  if (v.length() == 0) return false;
  if (v.equalsIgnoreCase("1")) return true;
  if (v.equalsIgnoreCase("true")) return true;
  if (v.equalsIgnoreCase("on")) return true;
  if (v.equalsIgnoreCase("yes")) return true;
  return false;
}

static bool json_get_str_key(const char* payload, size_t len, const char* key_with_quotes,
                             char* out, size_t out_sz) {
  if (!payload || !key_with_quotes || !out || out_sz == 0) return false;
  const char* p = strstr(payload, key_with_quotes);
  if (!p) return false;
  const char* q = strchr(p + strlen(key_with_quotes), '"');
  if (!q) return false;
  q++;
  const char* e = strchr(q, '"');
  if (!e) return false;
  size_t n = (size_t)(e - q);
  if (n >= out_sz) n = out_sz - 1;
  memcpy(out, q, n);
  out[n] = 0;
  (void)len;
  return true;
}

static bool json_get_bool_key(const char* payload, size_t len, const char* key_with_quotes, bool& out) {
  if (!payload || !key_with_quotes) return false;
  const char* p = strstr(payload, key_with_quotes);
  if (!p) return false;
  const char* c = strchr(p, ':');
  if (!c) return false;
  c++;
  while (*c && isspace((unsigned char)*c)) c++;
  if (!strncmp(c, "true", 4)) { out = true; return true; }
  if (!strncmp(c, "false", 5)) { out = false; return true; }
  if (*c == '1') { out = true; return true; }
  if (*c == '0') { out = false; return true; }
  (void)len;
  return false;
}

static bool kpi_build_payload(const SgCoreKpi* kpi, char* out, size_t out_sz) {
  if (!kpi || !out || out_sz == 0) return false;
  int n = snprintf(out, out_sz,
    "{\"window_ms\":%lu,\"meas_total\":%lu,\"meas_valid\":%lu,\"snr_avg\":%.3f,"
    "\"latency_avg_ms\":%lu,\"latency_max_ms\":%lu,"
    "\"state_ratio\":{\"empty\":%.3f,\"presence\":%.3f,\"motion\":%.3f},"
    "\"transition_count\":%lu,\"fp_proxy_ratio\":%.3f,\"fn_proxy_ratio\":%.3f}",
    (unsigned long)kpi->window_ms,
    (unsigned long)kpi->meas_total,
    (unsigned long)kpi->meas_valid,
    kpi->snr_avg,
    (unsigned long)kpi->latency_avg_ms,
    (unsigned long)kpi->latency_max_ms,
    kpi->state_ratio_empty,
    kpi->state_ratio_presence,
    kpi->state_ratio_motion,
    (unsigned long)kpi->transition_count,
    kpi->fp_proxy_ratio,
    kpi->fn_proxy_ratio
  );
  return (n > 0);
}

static void handle_http_occupancy() {
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) return;
  sg_http_next_seq(ctx);
  SgHttpOccupancy occ;
  occ.ts_ms = millis();
  const SgCoreSnapshot* snap = sg_core_get_snapshot();
  occ.state = snap ? (int)snap->pipe.stable : 0;
  occ.confidence = (snap && snap->pipe.stable == SG_EMPTY) ? 0.1f : 0.9f;
  char buf[256];
  sg_http_make_occupancy(ctx, &occ, buf, sizeof(buf));
  http_send_json(buf);
}

static void handle_http_tracks() {
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) return;
  sg_http_next_seq(ctx);
  SgHttpTracks tr;
  tr.ts_ms = millis();
  tr.count_active = 0; // placeholder
  char buf[192];
  sg_http_make_tracks(ctx, &tr, buf, sizeof(buf));
  http_send_json(buf);
}

static void handle_http_health() {
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) return;
  sg_http_next_seq(ctx);
  SgHttpHealth h;
  h.ts_ms = millis();
  h.uptime_s = (uint32_t)(millis() / 1000);
  h.rssi_dbm = 0;
  char buf[192];
  sg_http_make_health(ctx, &h, buf, sizeof(buf));
  http_send_json(buf);
}

static void handle_http_meas() {
  const SgCoreSnapshot* snap = sg_core_get_snapshot();
  if (!snap || !snap->has_meas) {
    http_set_cors();
    g_http_server.send(404, "application/json", "{\"error\":\"no_data\"}");
    return;
  }
  char buf[256];
  int n = snprintf(buf, sizeof(buf),
    "{\"ts_ms\":%lu,\"status\":\"%s\",\"dist_m\":%.3f,\"speed_mps\":%.3f,"
    "\"snr\":%.3f,\"distance_cm\":%u,\"speed_cms\":%d,\"signal\":%u,"
    "\"az_deg\":%d,\"el_deg\":%d,"
    "\"state\":%d,\"stable\":%d,\"stable_ms\":%lu,\"in_range\":%s}",
    (unsigned long)snap->meas_ms,
    status_str(snap->meas.status),
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
  (void)n;
  http_send_json(buf);
}

static void handle_http_kpi() {
  const SgCoreKpi* kpi = sg_core_kpi_last();
  if (!kpi || kpi->window_end_ms == 0) {
    http_set_cors();
    g_http_server.send(404, "application/json", "{\"error\":\"kpi_not_ready\"}");
    return;
  }
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) return;
  sg_http_next_seq(ctx);
  char payload[320];
  if (!kpi_build_payload(kpi, payload, sizeof(payload))) {
    http_set_cors();
    g_http_server.send(500, "application/json", "{\"error\":\"kpi_build_failed\"}");
    return;
  }
  char env[512];
  sg_http_envelope(ctx, kpi->window_end_ms, "kpi", payload, env, sizeof(env));
  http_send_json(env);
}

static void handle_http_options() {
  http_set_cors();
  g_http_server.send(204);
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
  SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
  if (!ctx) return;
  sg_http_next_seq(ctx);
  char buf[192];
  sg_http_make_ack(ctx, millis(), txid, 1, buf, sizeof(buf));
  http_send_json(buf);
}

static void handle_http_info() {
  http_set_cors();
  if (g_net_info) net_service_refresh(g_net_info);
  char buf[192];
  IPAddress sta;
  IPAddress ap;
  const char* ssid = "";
  if (g_net_info) {
    sta = g_net_info->sta_ip;
    ap  = g_net_info->ap_ip;
    ssid = g_net_info->ssid_sta;
  }
  snprintf(buf, sizeof(buf),
    "{\"device_id\":\"%s\",\"sta_ip\":\"%s\",\"ap_ip\":\"%s\",\"ssid_sta\":\"%s\"}",
    sg_telemetry_device_id(g_tctx),
    ip_to_str(sta).c_str(),
    ip_to_str(ap).c_str(),
    ssid ? ssid : "");
  g_http_server.send(200, "application/json", buf);
}

static void net_build_status_json(char* buf, size_t buf_sz) {
  if (!buf || buf_sz == 0) return;
  if (g_net_info) net_service_refresh(g_net_info);
  bool sta_set = net_service_has_sta_credentials();
  bool ap_set = net_service_has_ap_credentials();
  bool sta_connected = (g_net_info && g_net_info->sta_connected);
  IPAddress sta;
  IPAddress ap;
  if (g_net_info) {
    sta = g_net_info->sta_ip;
    ap  = g_net_info->ap_ip;
  }
  String sta_ip = ip_to_str(sta);
  String ap_ip  = ip_to_str(ap);
  snprintf(buf, buf_sz,
           "{\"sta_set\":%s,\"ap_set\":%s,\"sta_connected\":%s,\"sta_ip\":\"%s\",\"ap_ip\":\"%s\"}",
           sta_set ? "true" : "false",
           ap_set ? "true" : "false",
           sta_connected ? "true" : "false",
           sta_ip.c_str(),
           ap_ip.c_str());
}

static bool net_apply_from_form() {
  bool changed = false;
  if (parse_bool_str(g_http_server.arg("clear"))) {
    sg_net_clear_sta_credentials();
    sg_net_clear_ap_credentials();
    changed = true;
  }
  String sta_ssid = g_http_server.arg("sta_ssid");
  String sta_pass = g_http_server.arg("sta_pass");
  if (sta_ssid.length() > 0) {
    sg_net_set_sta_credentials(sta_ssid.c_str(), sta_pass.c_str());
    changed = true;
  }
  String ap_ssid = g_http_server.arg("ap_ssid");
  String ap_pass = g_http_server.arg("ap_pass");
  if (ap_ssid.length() > 0) {
    sg_net_set_ap_credentials(ap_ssid.c_str(), ap_pass.c_str());
    changed = true;
  }
  return changed;
}

static bool net_apply_from_json(const char* payload, size_t len) {
  bool changed = false;
  bool clear = false;
  if (json_get_bool_key(payload, len, "\"clear\"", clear) && clear) {
    sg_net_clear_sta_credentials();
    sg_net_clear_ap_credentials();
    changed = true;
  }
  char sta_ssid[33] = {0};
  char sta_pass[65] = {0};
  bool has_sta = json_get_str_key(payload, len, "\"sta_ssid\"", sta_ssid, sizeof(sta_ssid));
  bool has_sta_pass = json_get_str_key(payload, len, "\"sta_pass\"", sta_pass, sizeof(sta_pass));
  if (has_sta) {
    sg_net_set_sta_credentials(sta_ssid, has_sta_pass ? sta_pass : "");
    changed = true;
  }
  char ap_ssid[33] = {0};
  char ap_pass[65] = {0};
  bool has_ap = json_get_str_key(payload, len, "\"ap_ssid\"", ap_ssid, sizeof(ap_ssid));
  bool has_ap_pass = json_get_str_key(payload, len, "\"ap_pass\"", ap_pass, sizeof(ap_pass));
  if (has_ap) {
    sg_net_set_ap_credentials(ap_ssid, has_ap_pass ? ap_pass : "");
    changed = true;
  }
  return changed;
}

static void handle_http_net_get() {
  http_set_cors();
  char buf[192];
  net_build_status_json(buf, sizeof(buf));
  g_http_server.send(200, "application/json", buf);
}

static void handle_http_net_post() {
  bool changed = false;
  if (g_http_server.hasArg("sta_ssid") || g_http_server.hasArg("ap_ssid") || g_http_server.hasArg("clear")) {
    changed = net_apply_from_form();
  } else {
    String body = read_body();
    changed = net_apply_from_json(body.c_str(), body.length());
  }
  if (changed) {
    net_service_apply();
  }
  char buf[192];
  net_build_status_json(buf, sizeof(buf));
  http_set_cors();
  g_http_server.send(200, "application/json", buf);
}

static void handle_http_setup_get() {
  char buf[1024];
  const char* device_id = sg_telemetry_device_id(g_tctx);
  bool sta_set = net_service_has_sta_credentials();
  if (g_net_info) net_service_refresh(g_net_info);
  IPAddress sta;
  IPAddress ap;
  if (g_net_info) {
    sta = g_net_info->sta_ip;
    ap  = g_net_info->ap_ip;
  }
  String sta_ip = ip_to_str(sta);
  String ap_ip  = ip_to_str(ap);
  const char* status = sta_set ? "provisioned" : "not provisioned";
  int n = snprintf(buf, sizeof(buf),
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<title>SenseGrid Setup</title>"
    "<style>body{font-family:Arial,sans-serif;max-width:520px;margin:24px auto;padding:0 12px;}"
    "label{display:block;margin-top:12px;}input{width:100%%;padding:6px;}button{margin-top:12px;padding:8px 12px;}</style>"
    "</head><body>"
    "<h2>SenseGrid WiFi Setup</h2>"
    "<p>Device: %s</p>"
    "<p>Status: %s</p>"
    "<p>STA IP: %s | AP IP: %s</p>"
    "<form method=\"POST\" action=\"/setup\">"
    "<label>STA SSID</label><input name=\"sta_ssid\" type=\"text\" maxlength=\"32\">"
    "<label>STA Password</label><input name=\"sta_pass\" type=\"password\" maxlength=\"64\">"
    "<label>AP SSID (optional)</label><input name=\"ap_ssid\" type=\"text\" maxlength=\"32\">"
    "<label>AP Password (optional)</label><input name=\"ap_pass\" type=\"password\" maxlength=\"64\">"
    "<label><input type=\"checkbox\" name=\"clear\" value=\"1\"> Clear stored credentials</label>"
    "<button type=\"submit\">Save</button>"
    "</form>"
    "<p>After save, the device will reapply WiFi and may disconnect.</p>"
    "</body></html>",
    device_id ? device_id : "",
    status,
    sta_ip.c_str(),
    ap_ip.c_str());
  (void)n;
  g_http_server.send(200, "text/html", buf);
}

static void handle_http_setup_post() {
  bool changed = net_apply_from_form();
  const char* msg = changed ? "Saved. Reapplying WiFi..." : "No changes.";
  char buf[512];
  int n = snprintf(buf, sizeof(buf),
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<title>SenseGrid Setup</title></head><body>"
    "<p>%s</p><p><a href=\"/setup\">Back</a></p>"
    "</body></html>", msg);
  (void)n;
  g_http_server.send(200, "text/html", buf);
  if (changed) {
    delay(200);
    net_service_apply();
  }
}

static void handle_http_pipe() {
  http_set_cors();
  // se vier query ?state=on/off, ajusta
  if (g_http_server.hasArg("state")) {
    String st = g_http_server.arg("state");
    if (st.equalsIgnoreCase("on"))  sg_core_set_pipe_enabled(true, true);
    if (st.equalsIgnoreCase("off")) sg_core_set_pipe_enabled(false, true);
  }
  char buf[64];
  snprintf(buf, sizeof(buf), "{\"enabled\":%s}", sg_core_pipe_enabled() ? "true" : "false");
  g_http_server.send(200, "application/json", buf);
}

static void handle_http_diagnostics_page() {
  static const char k_html[] =
    "<!doctype html><html><head><meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>SenseGrid Diagnostics</title>"
    "<style>"
    ":root{--bg0:#0b1220;--bg1:#0f1a2b;--bg2:#16243a;--ink:#e6eef8;"
    "--muted:#9fb1c9;--accent:#2dd4bf;--accent2:#f59e0b;--card:#0f1a2b;"
    "--stroke:rgba(255,255,255,0.08);--glow:rgba(45,212,191,0.25);}"
    "*{box-sizing:border-box}body{margin:0;font-family:\"Space Grotesk\",\"Avenir Next\",\"Trebuchet MS\",sans-serif;"
    "color:var(--ink);background:radial-gradient(1200px 600px at 10% -10%,#1b2b45 0%,transparent 60%),"
    "radial-gradient(900px 500px at 90% 10%,#1a2d3f 0%,transparent 55%),"
    "linear-gradient(180deg,var(--bg0),var(--bg2));min-height:100vh;}"
    ".wrap{max-width:1100px;margin:0 auto;padding:28px 18px 60px;}"
    "header{display:flex;gap:16px;align-items:center;justify-content:space-between;margin-bottom:18px;}"
    ".title{font-size:28px;font-weight:600;letter-spacing:0.4px;}"
    ".subtitle{color:var(--muted);font-size:14px;}"
    ".chip{padding:6px 10px;border:1px solid var(--stroke);border-radius:999px;"
    "background:rgba(255,255,255,0.02);font-size:12px;}"
    ".grid{display:grid;grid-template-columns:repeat(12,1fr);gap:14px;}"
    ".card{grid-column:span 4;background:var(--card);border:1px solid var(--stroke);"
    "border-radius:16px;padding:16px;box-shadow:0 10px 30px rgba(0,0,0,0.25);"
    "position:relative;overflow:hidden;animation:fadeUp 600ms ease both;}"
    ".card.wide{grid-column:span 12;}"
    ".card::after{content:\"\";position:absolute;inset:auto -40% -60% -40%;height:120px;"
    "background:radial-gradient(120px 60px at 30% 20%,var(--glow),transparent 70%);opacity:0.6;}"
    ".label{font-size:12px;color:var(--muted);text-transform:uppercase;letter-spacing:1.2px;}"
    ".value{font-size:28px;font-weight:600;margin-top:6px;}"
    ".mono{font-family:\"Azeret Mono\",\"JetBrains Mono\",\"Courier New\",monospace;}"
    ".row{display:flex;gap:10px;flex-wrap:wrap;align-items:center;}"
    ".kv{display:flex;justify-content:space-between;gap:12px;margin-top:8px;"
    "font-size:13px;color:var(--muted);}"
    ".bar{margin-top:10px;height:12px;border-radius:999px;background:rgba(255,255,255,0.06);"
    "overflow:hidden;display:flex;}"
    ".seg{height:100%;transition:width 300ms ease;}"
    ".seg.empty{background:#64748b;} .seg.presence{background:var(--accent);} .seg.motion{background:var(--accent2);}"
    ".pulse{display:inline-flex;align-items:center;gap:6px;}"
    ".dot{width:8px;height:8px;border-radius:50%;background:var(--accent);box-shadow:0 0 10px var(--accent);}"
    ".muted{color:var(--muted);font-size:12px;}"
    "@keyframes fadeUp{from{opacity:0;transform:translateY(10px)}to{opacity:1;transform:translateY(0)}}"
    "@media (max-width:860px){.card{grid-column:span 6}.title{font-size:24px}}"
    "@media (max-width:560px){.card{grid-column:span 12}.title{font-size:22px}}"
    "</style></head><body>"
    "<div class=\"wrap\">"
    "<header>"
    "<div><div class=\"title\">SenseGrid Diagnostics</div>"
    "<div class=\"subtitle\">Local status and KPI snapshot</div></div>"
    "<div class=\"row\">"
    "<div class=\"chip mono\" id=\"dev_id\">device: --</div>"
    "<div class=\"chip\" id=\"last_upd\">updated: --</div>"
    "</div>"
    "</header>"

    "<section class=\"grid\">"
    "<div class=\"card\">"
    "<div class=\"label\">Health</div>"
    "<div class=\"value pulse\"><span class=\"dot\"></span><span id=\"health_status\">OK</span></div>"
    "<div class=\"kv\"><span>uptime</span><span class=\"mono\" id=\"uptime\">--</span></div>"
    "<div class=\"kv\"><span>rssi</span><span class=\"mono\" id=\"rssi\">--</span></div>"
    "</div>"

    "<div class=\"card\">"
    "<div class=\"label\">KPI window</div>"
    "<div class=\"value mono\" id=\"window_ms\">--</div>"
    "<div class=\"kv\"><span>meas total</span><span class=\"mono\" id=\"meas_total\">--</span></div>"
    "<div class=\"kv\"><span>meas valid</span><span class=\"mono\" id=\"meas_valid\">--</span></div>"
    "</div>"

    "<div class=\"card\">"
    "<div class=\"label\">SNR</div>"
    "<div class=\"value mono\" id=\"snr_avg\">--</div>"
    "<div class=\"kv\"><span>fp proxy</span><span class=\"mono\" id=\"fp_ratio\">--</span></div>"
    "<div class=\"kv\"><span>fn proxy</span><span class=\"mono\" id=\"fn_ratio\">--</span></div>"
    "</div>"

    "<div class=\"card\">"
    "<div class=\"label\">Latency (gap)</div>"
    "<div class=\"value mono\" id=\"lat_avg\">--</div>"
    "<div class=\"kv\"><span>max</span><span class=\"mono\" id=\"lat_max\">--</span></div>"
    "<div class=\"kv\"><span>transitions</span><span class=\"mono\" id=\"transitions\">--</span></div>"
    "</div>"

    "<div class=\"card\">"
    "<div class=\"label\">State ratio</div>"
    "<div class=\"value mono\" id=\"ratio_text\">--</div>"
    "<div class=\"bar\"><div class=\"seg empty\" id=\"seg_empty\"></div>"
    "<div class=\"seg presence\" id=\"seg_presence\"></div>"
    "<div class=\"seg motion\" id=\"seg_motion\"></div></div>"
    "<div class=\"kv\"><span>empty</span><span class=\"mono\" id=\"ratio_empty\">--</span></div>"
    "<div class=\"kv\"><span>presence</span><span class=\"mono\" id=\"ratio_presence\">--</span></div>"
    "<div class=\"kv\"><span>motion</span><span class=\"mono\" id=\"ratio_motion\">--</span></div>"
    "</div>"

    "<div class=\"card\">"
    "<div class=\"label\">Endpoints</div>"
    "<div class=\"value mono\">/v1/diagnostics</div>"
    "<div class=\"kv\"><span>status</span><span class=\"mono\">/status</span></div>"
    "<div class=\"kv\"><span>kpi</span><span class=\"mono\">/kpi</span></div>"
    "<div class=\"kv\"><span>export</span><span class=\"mono\">/export</span></div>"
    "</div>"

    "<div class=\"card wide\">"
    "<div class=\"label\">Notes</div>"
    "<div class=\"muted\">KPI updates once per window (default 60s)."
    " If KPI is not ready, the cards keep last data.</div>"
    "</div>"
    "</section>"
    "</div>"

    "<script>"
    "const $=id=>document.getElementById(id);"
    "const fmtPct=v=>isFinite(v)?(v*100).toFixed(1)+\"%\":\"--\";"
    "const fmtMs=v=>isFinite(v)?(v+\" ms\"):\"--\";"
    "const fmtS=v=>isFinite(v)?(v+\" s\"):\"--\";"
    "function setText(id,val){const el=$(id);if(el)el.textContent=val;}"
    "async function fetchJson(url){try{const r=await fetch(url);if(!r.ok)throw new Error(r.status);return await r.json();}"
    "catch(e){return null;}}"
    "async function updateInfo(){const j=await fetchJson('/v1/info');"
    "if(j&&j.device_id){setText('dev_id','device: '+j.device_id);}}"
    "async function updateHealth(){const j=await fetchJson('/v1/diagnostics/status');"
    "if(!j||!j.payload)return;const p=j.payload;"
    "setText('uptime',fmtS(p.uptime_s));"
    "setText('rssi',isFinite(p.rssi_dbm)?(p.rssi_dbm+\" dBm\"):\"--\");"
    "setText('health_status','OK');}"
    "function setRatios(r){"
    "const e=r.empty||0,p=r.presence||0,m=r.motion||0;"
    "setText('ratio_empty',fmtPct(e));setText('ratio_presence',fmtPct(p));setText('ratio_motion',fmtPct(m));"
    "setText('ratio_text',fmtPct(e)+' / '+fmtPct(p)+' / '+fmtPct(m));"
    "const total=e+p+m||1;"
    "$('seg_empty').style.width=((e/total)*100)+'%';"
    "$('seg_presence').style.width=((p/total)*100)+'%';"
    "$('seg_motion').style.width=((m/total)*100)+'%';}"
    "async function updateKpi(){const j=await fetchJson('/v1/diagnostics/kpi');"
    "if(!j||!j.payload)return;const p=j.payload;"
    "setText('window_ms',isFinite(p.window_ms)?(p.window_ms+' ms'):'--');"
    "setText('meas_total',isFinite(p.meas_total)?p.meas_total:'--');"
    "setText('meas_valid',isFinite(p.meas_valid)?p.meas_valid:'--');"
    "setText('snr_avg',isFinite(p.snr_avg)?p.snr_avg.toFixed(3):'--');"
    "setText('fp_ratio',fmtPct(p.fp_proxy_ratio));setText('fn_ratio',fmtPct(p.fn_proxy_ratio));"
    "setText('lat_avg',fmtMs(p.latency_avg_ms));setText('lat_max',fmtMs(p.latency_max_ms));"
    "setText('transitions',isFinite(p.transition_count)?p.transition_count:'--');setRatios(p.state_ratio||{});"
    "setText('last_upd','updated: '+new Date().toLocaleTimeString());}"
    "updateInfo();updateHealth();updateKpi();"
    "setInterval(updateHealth,3000);setInterval(updateKpi,5000);setInterval(updateInfo,15000);"
    "</script></body></html>";
  g_http_server.send(200, "text/html", k_html);
}

static void handle_http_monitor_page() {
  g_http_server.send(200, "text/html", k_monitor_html);
}

static void handle_http_diagnostics_export() {
  http_set_cors();
  int was_enabled = sg_adapters_logger_enabled();
  sg_adapters_logger_set_enabled(0);
  unsigned long total = sg_adapters_logger_size();
  g_http_server.setContentLength(total);
  g_http_server.send(200, "application/x-ndjson", "");
  WiFiClient client = g_http_server.client();
  sg_adapters_logger_export(client);
  sg_adapters_logger_set_enabled(was_enabled);
}

void http_service_init(SgTelemetryCtx* tctx, SgNetInfo* net_info) {
  g_tctx = tctx;
  g_net_info = net_info;

  g_http_server.on("/", HTTP_GET, handle_http_setup_get);
  g_http_server.on("/setup", HTTP_GET, handle_http_setup_get);
  g_http_server.on("/setup", HTTP_POST, handle_http_setup_post);
  g_http_server.on("/v1/occupancy", HTTP_GET, handle_http_occupancy);
  g_http_server.on("/v1/occupancy", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/tracks", HTTP_GET, handle_http_tracks);
  g_http_server.on("/v1/tracks", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/health", HTTP_GET, handle_http_health);
  g_http_server.on("/v1/health", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/diagnostics/status", HTTP_GET, handle_http_health);
  g_http_server.on("/v1/diagnostics/status", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/meas", HTTP_GET, handle_http_meas);
  g_http_server.on("/v1/meas", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/kpi", HTTP_GET, handle_http_kpi);
  g_http_server.on("/v1/kpi", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/diagnostics/kpi", HTTP_GET, handle_http_kpi);
  g_http_server.on("/v1/diagnostics/kpi", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/diagnostics/export", HTTP_GET, handle_http_diagnostics_export);
  g_http_server.on("/v1/diagnostics/export", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/diagnostics", HTTP_GET, handle_http_diagnostics_page);
  g_http_server.on("/monitor", HTTP_GET, handle_http_monitor_page);
  g_http_server.on("/dashboard", HTTP_GET, handle_http_monitor_page);
  g_http_server.on("/v1/cmd", HTTP_POST, handle_http_cmd);
  g_http_server.on("/v1/cmd", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/info", HTTP_GET, handle_http_info);
  g_http_server.on("/v1/info", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/net", HTTP_GET, handle_http_net_get);
  g_http_server.on("/v1/net", HTTP_POST, handle_http_net_post);
  g_http_server.on("/v1/net", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/pipe", HTTP_GET, handle_http_pipe);
  g_http_server.on("/v1/pipe", HTTP_OPTIONS, handle_http_options);
  g_http_server.begin();
  g_ws_server.begin();
  for (int i = 0; i < (int)(sizeof(g_ws_conns)/sizeof(g_ws_conns[0])); ++i) {
    g_ws_conns[i].active = false;
  }
  g_ws_last_emit = millis();
}

void http_service_loop(const SgCoreSnapshot* snap) {
  g_http_server.handleClient();
  ws_accept_clients();
  unsigned long now = millis();
  if (now - g_ws_last_emit >= WS_PERIOD_MS) {
    g_ws_last_emit = now;
    SgHttpCtx* ctx = sg_telemetry_http_ctx(g_tctx);
    if (!ctx) return;
    sg_http_next_seq(ctx);
    SgHttpOccupancy occ;
    occ.ts_ms = now;
    occ.state = (snap ? (int)snap->pipe.stable : 0);
    occ.confidence = (snap && snap->pipe.stable == SG_EMPTY) ? 0.1f : 0.9f;
    char buf[256];
    sg_http_make_occupancy(ctx, &occ, buf, sizeof(buf));
    ws_broadcast(buf);
  }
}
