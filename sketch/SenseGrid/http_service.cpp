#include "http_service.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <mbedtls/base64.h>
#include <mbedtls/sha1.h>
#include <string.h>
#include "net_service.h"

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

void http_service_init(SgTelemetryCtx* tctx, SgNetInfo* net_info) {
  g_tctx = tctx;
  g_net_info = net_info;

  g_http_server.on("/v1/occupancy", HTTP_GET, handle_http_occupancy);
  g_http_server.on("/v1/occupancy", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/tracks", HTTP_GET, handle_http_tracks);
  g_http_server.on("/v1/tracks", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/health", HTTP_GET, handle_http_health);
  g_http_server.on("/v1/health", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/meas", HTTP_GET, handle_http_meas);
  g_http_server.on("/v1/meas", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/cmd", HTTP_POST, handle_http_cmd);
  g_http_server.on("/v1/cmd", HTTP_OPTIONS, handle_http_options);
  g_http_server.on("/v1/info", HTTP_GET, handle_http_info);
  g_http_server.on("/v1/info", HTTP_OPTIONS, handle_http_options);
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
