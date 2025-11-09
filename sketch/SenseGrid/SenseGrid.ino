// sketch/SenseGrid/SenseGrid.ino — A2: CLI + LOG embutidos + stream/json/rate + filtro de alcance
// Alvo: ESP32-C3 (Serial = USB p/ log/CLI, Serial1 = UART do radar)

#include <Arduino.h>
#include "pins_radar.h"
#include "glue/hal_uart_glue.h"
#include "glue/drv_radar_me73_glue.h"
#include "glue/ring_samples_glue.h"

// ===================== LOG (embutido) =====================
enum { SG_ERROR=0, SG_WARN=1, SG_INFO=2, SG_DEBUG=3 };
static int g_log_level = SG_INFO;
static const char* lvl_str(int l){
  switch(l){case 0:return "ERROR";case 1:return "WARN";case 2:return "INFO";default:return "DEBUG";}
}
#define SG_LOG_INIT()            do{ g_log_level = SG_INFO; }while(0)
#define SG_LOG_SET_LEVEL(lvl)    do{ g_log_level = (lvl); }while(0)
#define SG_LOG(lvl, fmt, ...)    do{ if((lvl) <= g_log_level){ Serial.printf("[%s] " fmt "\r\n", lvl_str(lvl), ##__VA_ARGS__);} }while(0)

// ===================== CLI (embutido) =====================
static bool     g_stream_on = false;
static bool     g_json_on   = true;
static uint32_t g_min_interval_ms = 0;     // 0 = sem limite
static uint16_t g_range_max_cm   = 200;    // filtro de alcance no APP (e também mando pro radar)

static char   cli_buf[96];
static size_t cli_len = 0;

static void cli_help() {
  Serial.println(F("Commands:"));
  Serial.println(F("  help                - show this help"));
  Serial.println(F("  info                - print current settings"));
  Serial.println(F("  stream on|off       - enable/disable streaming"));
  Serial.println(F("  json on|off         - JSON mode on/off"));
  Serial.println(F("  rate <ms>           - min interval between outputs (0 = no limit)"));
  Serial.println(F("  log <0..3>          - 0=error 1=warn 2=info 3=debug"));
  Serial.println(F("  range <cm>          - app filter AND set presence max in radar (saves)"));
}

static void cli_info() {
  Serial.println(F("SenseGrid CLI status:"));
  Serial.print  (F("  stream=")); Serial.println(g_stream_on ? "on" : "off");
  Serial.print  (F("  json="));   Serial.println(g_json_on ? "on" : "off");
  Serial.print  (F("  rate="));   Serial.println(g_min_interval_ms);
  Serial.print  (F("  range_max_cm=")); Serial.println(g_range_max_cm);
  Serial.print  (F("  log_level=")); Serial.println(g_log_level);
}

// ===================== App globals =====================
UartHandle  g_uart;
RadarHandle g_radar;

static SgSample g_samples[256];
SgRing g_ring;

static int last_occ = -1;

static const char* status_str(uint8_t s) {
  switch (s) { case 0: return "none"; case 1: return "move"; case 2: return "exist"; default: return "?"; }
}

static float snr_norm(uint16_t signal) { // 0..~2047 -> 0..1
  float v = signal / 2047.0f;
  if (v < 0) v = 0; if (v > 1) v = 1;
  return v;
}

// ===================== UART helpers (sketch side) =====================
static void uart_drain_rx(UartHandle* h, uint32_t max_ms=50) {
  if (!h || !h->impl) return;
  HardwareSerial* ser = reinterpret_cast<HardwareSerial*>(h->impl);
  uint32_t t0 = millis();
  while (millis() - t0 < max_ms) { while (ser->available() > 0) { ser->read(); } delay(2); }
}

static void radar_send(const uint8_t* cmd, size_t n) {
  if (!g_uart.impl) return;
  uart_write(&g_uart, cmd, n);
  delay(40);
  uart_drain_rx(&g_uart, 60);
}

static uint8_t sum8_cfg(uint8_t func, uint8_t c1, uint8_t c2, const uint8_t* p, size_t n){
  uint32_t s = func + c1 + c2;
  for(size_t i=0;i<n;i++) s += p[i];
  return (uint8_t)(s & 0xFF);
}

// ===== Envia VO hold (ms) e Presence Max (cm) =====
static void radar_cfg_vo_hold_ms(uint16_t ms){
  // 55 5A 00 06 01 80 14 <ms_hi> <ms_lo> <sum>
  uint8_t buf[] = {0x55,0x5A,0x00,0x06,0x01,0x80,0x14,0x00,0x00,0x00};
  buf[7] = (uint8_t)(ms >> 8);
  buf[8] = (uint8_t)(ms & 0xFF);
  buf[9] = sum8_cfg(0x01,0x80,0x14,&buf[7],2);
  radar_send(buf, sizeof(buf));
}

static void radar_cfg_presence_max_cm(uint16_t cm){
  // 55 5A 00 06 01 80 0E <cm_hi> <cm_lo> <sum>
  uint8_t buf[] = {0x55,0x5A,0x00,0x06,0x01,0x80,0x0E,0x00,0x00,0x00};
  buf[7] = (uint8_t)(cm >> 8);
  buf[8] = (uint8_t)(cm & 0xFF);
  buf[9] = sum8_cfg(0x01,0x80,0x0E,&buf[7],2);
  radar_send(buf, sizeof(buf));
}

static void radar_cfg_save_all(){
  // 55 5A 00 04 01 20 04 D8
  static const uint8_t SAVE_ALL[] = {0x55,0x5A,0x00,0x04,0x01,0x20,0x04,0xD8};
  radar_send(SAVE_ALL, sizeof(SAVE_ALL));
}

// ===== Config no boot =====
static uint32_t g_after_cfg_ms = 0;

static void radar_config_boot() {
  SG_LOG(SG_INFO, "[CFG] VO hold = 1000 ms");
  radar_cfg_vo_hold_ms(1000);

  SG_LOG(SG_INFO, "[CFG] Presence max = %ucm (logica interna)", g_range_max_cm);
  radar_cfg_presence_max_cm(g_range_max_cm);

  SG_LOG(SG_INFO, "[CFG] Save all");
  radar_cfg_save_all();

  g_after_cfg_ms = millis() + 1000; // 1 s warm-up
}

// ===================== Telemetria =====================
static RadarParsed g_last;
static bool     g_has_last = false;
static uint32_t g_last_seen_ms = 0;
static uint32_t g_tick_ms = 0;
static const uint32_t TICK_PERIOD_MS = 250;
static const uint32_t STALE_MS = 3000;
static uint32_t g_last_stream_ms = 0;

static void make_effective(const RadarParsed& in, uint8_t& eff_status, uint16_t& eff_dist_cm, uint16_t& eff_signal) {
  if (in.distance_cm == 0 || in.distance_cm > g_range_max_cm) {
    eff_status = 0; eff_dist_cm = 0; eff_signal = 0;
  } else {
    eff_status = in.status;
    eff_dist_cm = in.distance_cm;
    eff_signal  = in.signal;
  }
}

// ===================== CLI parsing =====================
static void cli_handle_line(const char* s){
  if (!s || !*s) return;

  if (!strcasecmp(s,"help")) { cli_help(); return; }
  if (!strcasecmp(s,"info")) { cli_info(); return; }

  if (!strncasecmp(s,"stream ",7)) {
    const char* v = s+7;
    if (!strcasecmp(v,"on"))  { g_stream_on = true;  Serial.println(F("stream=on")); }
    else if (!strcasecmp(v,"off")) { g_stream_on = false; Serial.println(F("stream=off")); }
    else Serial.println(F("usage: stream on|off"));
    return;
  }

  if (!strncasecmp(s,"json ",5)) {
    const char* v = s+5;
    if (!strcasecmp(v,"on"))  { g_json_on = true;  Serial.println(F("json=on")); }
    else if (!strcasecmp(v,"off")) { g_json_on = false; Serial.println(F("json=off")); }
    else Serial.println(F("usage: json on|off"));
    return;
  }

  if (!strncasecmp(s,"rate ",5)) {
    uint32_t ms = strtoul(s+5, nullptr, 10);
    g_min_interval_ms = ms;
    Serial.print(F("rate(ms)=")); Serial.println(g_min_interval_ms);
    return;
  }

  if (!strncasecmp(s,"log ",4)) {
    int lvl = atoi(s+4);
    if (lvl < 0) lvl = 0; if (lvl > 3) lvl = 3;
    SG_LOG_SET_LEVEL(lvl);
    Serial.print(F("log level=")); Serial.println(g_log_level);
    return;
  }

  if (!strncasecmp(s,"range ",6)) {
    uint16_t cm = (uint16_t)strtoul(s+6, nullptr, 10);
    if (cm < 50) cm = 50;                     // um limite mínimo razoável
    if (cm > 500) cm = 500;                   // teto seguro
    g_range_max_cm = cm;
    Serial.print(F("range_max_cm=")); Serial.println(g_range_max_cm);

    // também programa o radar e salva
    radar_cfg_presence_max_cm(g_range_max_cm);
    radar_cfg_save_all();
    Serial.println(F("(radar presence max atualizado e salvo)"));
    return;
  }

  Serial.println(F("?? comando desconhecido — digite 'help'"));
}

static void cli_poll() {
  while (Serial.available() > 0) {
    int c = Serial.read();
    if (c < 0) break;
    if (c == '\r') continue;
    if (c == '\n') {
      cli_buf[cli_len] = 0;
      cli_handle_line(cli_buf);
      cli_len = 0;
    } else {
      if (cli_len < sizeof(cli_buf)-1) cli_len += (cli_buf[cli_len] = (char)c, 1);
    }
  }
}

// ===================== setup/loop =====================
void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 1500) {}

  pinMode(SG_PIN_RADAR_OCC, INPUT_PULLDOWN);

  SG_LOG_INIT();
  SG_LOG(SG_INFO, "[BOOT] A2 PARSE + JSON + CLI embutidos (range=%ucm)", g_range_max_cm);

  if (!uart_begin(&g_uart, /*uart_num*/1, SG_RADAR_BAUD, /*RX*/SG_RADAR_RX, /*TX*/SG_RADAR_TX)) {
    SG_LOG(SG_ERROR, "[UART] uart_begin falhou");
  } else {
    SG_LOG(SG_INFO,  "[UART1] RX=%d TX=%d @%lu", SG_RADAR_RX, SG_RADAR_TX, (unsigned long)SG_RADAR_BAUD);
  }
  if (!radar_begin(&g_radar, &g_uart)) {
    SG_LOG(SG_ERROR, "[RADAR] radar_begin falhou");
  }

  ring_init(&g_ring, g_samples, 256);
  SG_LOG(SG_INFO,  "[OCC] pin=%d", SG_PIN_RADAR_OCC);

  radar_config_boot();
}

void loop() {
  // CLI (USB)
  cli_poll();

  // OCC digital
  int occ = digitalRead(SG_PIN_RADAR_OCC);
  if (occ != last_occ) {
    last_occ = occ;
    SG_LOG(SG_INFO, "[OCC] %d  t=%lu", occ, (unsigned long)millis());
  }

  // Leitura depois do warm-up
  if (millis() >= g_after_cfg_ms) {
    for (int i = 0; i < 3; ++i) {
      RadarParsed p;
      if (!radar_read_parsed(&g_radar, &p, /*timeout_ms*/20)) break;

      g_last = p;
      g_has_last = true;
      g_last_seen_ms = millis();

      // ring (cru)
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

      // Stream opcional (com filtro de alcance no app)
      if (g_stream_on) {
        if (g_min_interval_ms == 0 || (millis() - g_last_stream_ms) >= g_min_interval_ms) {
          g_last_stream_ms = millis();

          uint8_t st; uint16_t dcm; uint16_t sig;
          make_effective(p, st, dcm, sig);

          if (g_json_on) {
            Serial.print(F("{\"ts_ms\":")); Serial.print((unsigned long)g_last_stream_ms);
            Serial.print(F(",\"status\":\"")); Serial.print(status_str(st)); Serial.print(F("\""));
            Serial.print(F(",\"dist_m\":"));    Serial.print(dcm / 100.0f, 3);
            Serial.print(F(",\"speed_mps\":")); Serial.print(p.speed_cms / 100.0f, 3);
            Serial.print(F(",\"snr\":"));       Serial.print(snr_norm(sig), 3);
            Serial.print(F(",\"distance_cm\":")); Serial.print(dcm);
            Serial.print(F(",\"speed_cms\":"));   Serial.print(p.speed_cms);
            Serial.print(F(",\"signal\":"));      Serial.print(sig);
            Serial.println(F("}"));
          } else {
            Serial.print(F("[STREAM] ")); Serial.print(status_str(st));
            Serial.print(F("  dist="));    Serial.print(dcm); Serial.print(F("cm"));
            Serial.print(F("  sig="));     Serial.print(sig);
            Serial.print(F("  t="));       Serial.println((unsigned long)g_last_stream_ms);
          }
        }
      }
    }
  }

  // Ticker (debug): mostra fallback se ficar stale
  static const uint32_t TICK_PERIOD_MS = 250;
  static const uint32_t STALE_MS = 3000;
  static uint32_t g_tick_ms = 0;

  if (millis() - g_tick_ms >= TICK_PERIOD_MS) {
    g_tick_ms = millis();
    if (g_has_last) {
      bool stale = (millis() - g_last_seen_ms > STALE_MS);
      uint8_t shown;
      if (stale) {
        shown = (digitalRead(SG_PIN_RADAR_OCC) == 1) ? 2 /*exist*/ : 0 /*none*/;
      } else {
        uint8_t st; uint16_t dcm; uint16_t sig;
        make_effective(g_last, st, dcm, sig);
        shown = st;
      }
      SG_LOG(SG_DEBUG, "[TICK] %s t=%lu", status_str(shown), (unsigned long)millis());
    } else {
      SG_LOG(SG_DEBUG, "[TICK] aguardando primeira leitura...");
    }
  }

  // dreno do ring (placeholder)
  SgSample out; (void)ring_pop(&g_ring, &out);

  delay(2);
}
