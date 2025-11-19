// sketch/SenseGrid/SenseGrid.ino
// SenseGrid — ESP32-C3 + ME73MS01
// Fixes (quiet by default):
//  - Só faz stream PARSED/JSON quando `stream on` (CLI).
//  - TICK em nível DEBUG (LOGD), então só aparece com `log 3`.
//  - cli_help(Print&) / cli_info(Print&) com assinatura certa p/ sg_cli.h
//  - sg_cli_set_handlers com ponteiros de função (sem lambdas erradas)
//  - Range travado em 2.00 m (200 cm) com checksum correto

#include <Arduino.h>

#include "pins_radar.h"
#include "glue/hal_uart_glue.h"
#include "glue/drv_radar_me73_glue.h"
#include "glue/ring_samples_glue.h"
#include "glue/sg_cli_glue.h"    // sg_cli_set_handlers()/sg_cli_poll()
#include "glue/sg_pipe_glue.h"   // inclui pipeline e helpers

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
static bool  g_stream = false;          // << quieto por padrão
static bool  g_stream_json = true;
static unsigned long g_stream_period_ms = 50; // ~20 Hz

// pipeline
static SgPipeOut g_pipe_out;

// range gate (2.00 m)
static const uint16_t RANGE_CM = 200;

// timing
static const uint32_t TICK_PERIOD_MS = 250;
static const uint32_t STALE_MS       = 3000;

// warm-up pós config
static uint32_t g_after_cfg_ms = 0;

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
      (p.distance_cm>0 && p.distance_cm<=RANGE_CM)? "true":"false"
    );
  } else {
    Serial.printf("[PARSED] status=%s dist=%ucm signal=%u  state=%d stable=%d t=%lu\n",
                  status_str(p.status), p.distance_cm, p.signal,
                  (int)o.state, (int)o.stable, (unsigned long)millis());
  }
}

// ---------------------- Config do radar no boot ----------------------
// VO hold = 3000 ms (0x0BB8)
static const uint8_t SET_VO_HOLD_3S[]           = {0x55,0x5A,0x00,0x06,0x01,0x80,0x14,0x0B,0xB8,0x0D};
// Presence max = 200 cm (0x00C8), checksum = 0x0C
static const uint8_t SET_PRESENCE_MAX_200CM[]   = {0x55,0x5A,0x00,0x06,0x01,0x80,0x0E,0x00,0xC8,0x0C};
// Save all
static const uint8_t SAVE_ALL[]                 = {0x55,0x5A,0x00,0x04,0x01,0x20,0x04,0xD8};

static void radar_config_boot() {
  LOGI("[CFG] VO hold = 3000 ms");
  radar_send(SET_VO_HOLD_3S, sizeof(SET_VO_HOLD_3S));

  LOGI("[CFG] Presence max = %u cm", (unsigned)RANGE_CM);
  radar_send(SET_PRESENCE_MAX_200CM, sizeof(SET_PRESENCE_MAX_200CM));

  LOGI("[CFG] Save all");
  radar_send(SAVE_ALL, sizeof(SAVE_ALL));

  g_after_cfg_ms = millis() + 1000; // 1s
}

// ---------------------- CLI local (assinaturas do sg_cli.h) ----------------------
static void cli_help(Print& out) {
  out.println(F("SenseGrid CLI — comandos:"));
  out.println(F("  help            -> esta ajuda"));
  out.println(F("  info            -> info de build e pinos"));
  out.println(F("  stream on|off   -> liga/desliga streaming"));
  out.println(F("  rate <Hz>       -> taxa do stream (0=libera)"));
  out.println(F("  json on|off     -> formato JSON ou texto"));
  out.println(F("  log <0..3>      -> nivel de log (0=err..3=dbg)"));
}

static void cli_info(Print& out) {
  out.print(F("UART1 RX=")); out.print(SG_RADAR_RX);
  out.print(F(" TX=")); out.print(SG_RADAR_TX);
  out.print(F(" @")); out.println(SG_RADAR_BAUD);
  out.print(F("OCC pin=")); out.println(SG_PIN_RADAR_OCC);
  out.print(F("Range (max presence)=")); out.print(RANGE_CM); out.println(F(" cm"));
}

static void on_cli_stream(bool enable) { g_stream = enable; LOGI("[CLI] stream %s", enable ? "on" : "off"); }
static void on_cli_rate(unsigned long hz) {
  if (hz == 0) g_stream_period_ms = 0;
  else        g_stream_period_ms = (unsigned long)max(1UL, 1000UL / hz);
  LOGI("[CLI] rate = %lu Hz (period %lu ms)", hz, g_stream_period_ms);
}
static void on_cli_json(bool enable) { g_stream_json = enable; LOGI("[CLI] json %s", enable ? "on" : "off"); }
static void on_cli_log(int lvl) { g_log_level = constrain(lvl, 0, 3); LOGI("[CLI] log level = %d", g_log_level); }

// ---------------------- Setup/Loop ----------------------
void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 1500) { /* aguarda enumerar */ }

  pinMode(SG_PIN_RADAR_OCC, INPUT_PULLDOWN);

  if (!uart_begin(&g_uart, /*uart*/1, SG_RADAR_BAUD, /*RX*/SG_RADAR_RX, /*TX*/SG_RADAR_TX)) {
    LOGE("[ERR] uart_begin failed");
  }
  if (!radar_begin(&g_radar, &g_uart)) {
    LOGE("[ERR] radar_begin failed");
  }

  ring_init(&g_ring, g_samples, 256);
  sg_pipe_init(); // pipeline default

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
    /*log   */ on_cli_log
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
    for (int i = 0; i < 3; ++i) {
      RadarParsed p;
      if (!radar_read_parsed(&g_radar, &p, /*timeout_ms*/20)) break;

      g_last = p;
      g_has_last = true;
      g_last_seen_ms = millis();

      // pipeline (básico)
      SgPipeIn in { p.status, p.distance_cm, p.speed_cms, p.snr, g_last_seen_ms };
      g_pipe_out = sg_pipe_step(in);

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
      if (g_stream) {
        // respeita período se configurado
        static unsigned long last_emit = 0;
        unsigned long now = millis();
        if (g_stream_period_ms == 0 || (now - last_emit) >= g_stream_period_ms) {
          print_json(p, g_pipe_out);
          last_emit = now;
        }
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

  delay(2);
}
