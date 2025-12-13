#include "include/adapters_mqtt.h"
#include <string.h>
#include <stdlib.h>

static WiFiClient g_mqtt_client;
static SgMqttCfg g_cfg;
static SgMqttCmdCb g_cmd_cb = NULL;
static uint16_t g_pkt_id = 1;
static unsigned long g_last_ping = 0;
static unsigned long g_last_attempt = 0;
static unsigned long g_backoff_ms = 1000;

static char g_topic_buf[128];
static char g_hdr[10];

static uint16_t next_pkt_id() { g_pkt_id++; if (g_pkt_id == 0) g_pkt_id = 1; return g_pkt_id; }

static size_t encode_len(uint32_t len, uint8_t* out) {
  size_t i = 0;
  do {
    uint8_t byte = len % 128;
    len /= 128;
    if (len > 0) byte |= 0x80;
    out[i++] = byte;
  } while (len > 0 && i < 4);
  return i;
}

static bool mqtt_write(const uint8_t* data, size_t len) {
  size_t n = g_mqtt_client.write(data, len);
  return n == len;
}

static bool mqtt_send_connect() {
  uint8_t var[200];
  size_t idx = 0;
  // Protocol Name "MQTT"
  var[idx++] = 0; var[idx++] = 4;
  var[idx++] = 'M'; var[idx++] = 'Q'; var[idx++] = 'T'; var[idx++] = 'T';
  var[idx++] = 4; // level
  uint8_t flags = 0;
  if (g_cfg.user && g_cfg.user[0]) flags |= 0x80;
  if (g_cfg.pass && g_cfg.pass[0]) flags |= 0x40;
  flags |= 0x02; // clean session
  var[idx++] = flags;
  uint16_t ka = g_cfg.keepalive_s ? g_cfg.keepalive_s : 30;
  var[idx++] = (ka >> 8) & 0xFF; var[idx++] = ka & 0xFF;
  // Client ID
  size_t cid_len = strlen(g_cfg.device_id);
  var[idx++] = (cid_len >> 8) & 0xFF; var[idx++] = cid_len & 0xFF;
  memcpy(&var[idx], g_cfg.device_id, cid_len); idx += cid_len;
  if (g_cfg.user && g_cfg.user[0]) {
    size_t ulen = strlen(g_cfg.user);
    var[idx++] = (ulen >> 8) & 0xFF; var[idx++] = ulen & 0xFF;
    memcpy(&var[idx], g_cfg.user, ulen); idx += ulen;
  }
  if (g_cfg.pass && g_cfg.pass[0]) {
    size_t plen = strlen(g_cfg.pass);
    var[idx++] = (plen >> 8) & 0xFF; var[idx++] = plen & 0xFF;
    memcpy(&var[idx], g_cfg.pass, plen); idx += plen;
  }
  uint8_t rem[4]; size_t rem_len = encode_len(idx, rem);
  g_hdr[0] = 0x10; // CONNECT
  if (!mqtt_write((uint8_t*)g_hdr, 1)) return false;
  if (!mqtt_write(rem, rem_len)) return false;
  if (!mqtt_write(var, idx)) return false;
  return true;
}

static bool mqtt_send_ping() {
  uint8_t pkt[2] = {0xC0, 0x00};
  return mqtt_write(pkt, 2);
}

static bool mqtt_send_subscribe(const char* topic, uint8_t qos) {
  uint8_t var[200];
  size_t idx = 0;
  uint16_t pkt_id = next_pkt_id();
  var[idx++] = (pkt_id >> 8) & 0xFF; var[idx++] = pkt_id & 0xFF;
  size_t tlen = strlen(topic);
  var[idx++] = (tlen >> 8) & 0xFF; var[idx++] = tlen & 0xFF;
  memcpy(&var[idx], topic, tlen); idx += tlen;
  var[idx++] = qos;
  uint8_t rem[4]; size_t rem_len = encode_len(idx, rem);
  g_hdr[0] = 0x82; // SUBSCRIBE QoS1
  g_hdr[1] = 0; // placeholder, rem len follows
  if (!mqtt_write((uint8_t*)g_hdr, 1)) return false;
  if (!mqtt_write(rem, rem_len)) return false;
  if (!mqtt_write(var, idx)) return false;
  return true;
}

static bool mqtt_send_publish(const char* topic, const char* payload, bool retain, uint8_t qos) {
  size_t tlen = strlen(topic);
  size_t plen = strlen(payload);
  size_t rem_sz = 2 + tlen + plen + (qos ? 2 : 0);
  uint8_t rem[4]; size_t rem_len = encode_len(rem_sz, rem);
  uint8_t hdr = 0x30 | (retain ? 0x01 : 0) | (qos ? 0x02 : 0);
  if (!mqtt_write(&hdr, 1)) return false;
  if (!mqtt_write(rem, rem_len)) return false;
  uint8_t tl[2] = { (uint8_t)(tlen >> 8), (uint8_t)(tlen & 0xFF) };
  if (!mqtt_write(tl, 2)) return false;
  if (!mqtt_write((const uint8_t*)topic, tlen)) return false;
  if (qos) {
    uint16_t pid = next_pkt_id();
    uint8_t pidb[2] = { (uint8_t)(pid >> 8), (uint8_t)(pid & 0xFF) };
    if (!mqtt_write(pidb, 2)) return false;
  }
  if (!mqtt_write((const uint8_t*)payload, plen)) return false;
  return true;
}

static bool mqtt_connect_and_sub() {
  if (!g_mqtt_client.connect(g_cfg.host, g_cfg.port)) return false;
  if (!mqtt_send_connect()) { g_mqtt_client.stop(); return false; }
  // Wait for CONNACK (simple)
  unsigned long t0 = millis();
  while (g_mqtt_client.connected() && (millis() - t0) < 2000) {
    if (g_mqtt_client.available() >= 4) {
      uint8_t b0 = g_mqtt_client.read();
      uint8_t b1 = g_mqtt_client.read();
      uint8_t b2 = g_mqtt_client.read();
      uint8_t b3 = g_mqtt_client.read();
      (void)b0; (void)b1; (void)b2; (void)b3;
      break;
    }
  }
  // Subscribe cmd
  snprintf(g_topic_buf, sizeof(g_topic_buf), "sp%s/%s/c", g_cfg.sb_ref, g_cfg.device_id);
  mqtt_send_subscribe(g_topic_buf, 1);
  return true;
}

void sg_mqtt_init(const SgMqttCfg* cfg, SgMqttCmdCb cmd_cb) {
  if (!cfg) return;
  g_cfg = *cfg;
  g_cmd_cb = cmd_cb;
  g_pkt_id = 1;
  g_last_ping = millis();
  g_last_attempt = 0;
  g_backoff_ms = 1000;
}

bool sg_mqtt_connected() {
  return g_mqtt_client.connected();
}

void sg_mqtt_loop() {
  if (!g_mqtt_client.connected()) {
    unsigned long now = millis();
    if (now - g_last_attempt >= g_backoff_ms) {
      g_last_attempt = now;
      if (mqtt_connect_and_sub()) {
        g_backoff_ms = 1000;
      } else {
        if (g_backoff_ms < 30000) g_backoff_ms *= 2;
      }
    }
    return;
  }
  // keepalive
  unsigned long now = millis();
  if (now - g_last_ping >= (g_cfg.keepalive_s ? g_cfg.keepalive_s*500 : 15000)) {
    mqtt_send_ping();
    g_last_ping = now;
  }
  // process incoming (basic)
  while (g_mqtt_client.available() > 0) {
    uint8_t hdr = g_mqtt_client.read();
    uint8_t type = hdr >> 4;
    // remaining length (varint)
    uint32_t rem = 0; uint8_t mult=1; uint8_t b=0;
    do { b = g_mqtt_client.read(); rem += (b & 0x7F) * mult; mult *= 128; } while ((b & 0x80)!=0);
    if (type == 3) { // PUBLISH
      uint8_t t1 = g_mqtt_client.read();
      uint8_t t2 = g_mqtt_client.read();
      uint16_t tlen = ((uint16_t)t1 << 8) | t2;
      if (tlen >= sizeof(g_topic_buf)) { // skip
        for (uint16_t i=0;i<tlen;i++) g_mqtt_client.read();
        for (uint32_t i=0;i<rem-2-tlen;i++) g_mqtt_client.read();
        continue;
      }
      g_mqtt_client.readBytes((uint8_t*)g_topic_buf, tlen);
      g_topic_buf[tlen] = 0;
      uint16_t pid = 0;
      if (((hdr >>1)&0x03)==1) { // QoS1
        pid = ((uint16_t)g_mqtt_client.read() << 8) | g_mqtt_client.read();
        rem -= 2;
      }
      size_t pay_len = rem - 2 - tlen;
      static char payload[256];
      if (pay_len >= sizeof(payload)) pay_len = sizeof(payload)-1;
      g_mqtt_client.readBytes((uint8_t*)payload, pay_len);
      payload[pay_len] = 0;
      if (g_cmd_cb && strstr(g_topic_buf, "/c")) {
        g_cmd_cb(payload, pay_len);
      }
      if (((hdr >>1)&0x03)==1) {
        // send PUBACK
        uint8_t ack[4] = {0x40, 0x02, (uint8_t)(pid>>8), (uint8_t)(pid&0xFF)};
        mqtt_write(ack, 4);
      }
    } else {
      // skip bytes
      for (uint32_t i=0;i<rem;i++) g_mqtt_client.read();
    }
  }
}

bool sg_mqtt_publish(const char* topic_suf, const char* payload, bool retain, uint8_t qos) {
  if (!g_mqtt_client.connected()) return false;
  snprintf(g_topic_buf, sizeof(g_topic_buf), "sp%s/%s/%s", g_cfg.sb_ref, g_cfg.device_id, topic_suf);
  return mqtt_send_publish(g_topic_buf, payload, retain, qos);
}

bool sg_mqtt_pub_meas(const char* payload)   { return sg_mqtt_publish("meas",   payload, false, 0); }
bool sg_mqtt_pub_meas_raw(const char* payload) { return sg_mqtt_publish("meas_raw", payload, false, 0); }
bool sg_mqtt_pub_event(const char* payload)  { return sg_mqtt_publish("events", payload, false, 1); }
bool sg_mqtt_pub_status(const char* payload) { return sg_mqtt_publish("status", payload, true, 1); }
bool sg_mqtt_pub_cap(const char* payload)    { return sg_mqtt_publish("cap",    payload, true, 1); }
// Ack/err em QoS1 para garantir entrega do comando
bool sg_mqtt_pub_ack(const char* payload)    { return sg_mqtt_publish("ack",    payload, false, 1); }
bool sg_mqtt_pub_err(const char* payload)    { return sg_mqtt_publish("err",    payload, false, 1); }
