// sketch/SenseGrid/SenseGrid.ino — A2: CLI + SG_LOG + stream/json/rate (VO=1s, MAX=200cm)
#include <Arduino.h>
#include "pins_radar.h"
#include "glue/hal_uart_glue.h"
#include "glue/drv_radar_me73_glue.h"
#include "glue/ring_samples_glue.h"
#include "glue/sg_cli_glue.h"
#include "glue/sg_log_glue.h"

UartHandle  g_uart;
RadarHandle g_radar;

static SgSample g_samples[256];
SgRing g_ring;

static int last_occ = -1;

static const char* status_str(uint8_t s) {
  switch (s) {
    case 0: return "none";
    case 1: return "move";
    case 2: return "exist";
    default: return "?";
  }
}

// === Helpers UART (lado sketch) ===
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

// === Config no boot ===
static uint32_t g_after_cfg_ms = 0;

static void radar_config_boot() {
  // VO hold = 1000 ms (reduz o delay para voltar a "none")
  static const uint8_t SET_VO_HOLD_1S[]         = {0x55,0x5A,0x00,0x06,0x01,0x80,0x14,0x03,0xE8,0x35};
  // Presence max = 200 cm (2 m)
  static const uint8_t SET_PRES_MAX_200CM[]     = {0x55,0x5A,0x00,0x06,0x01,0x80,0x0E,0x00,0xC8,0x0C};
  // Save all
  static const uint8_t SAVE_ALL[]               = {0x55,0x5A,0x00,0x04,0x01,0x20,0x04,0xD8};

  SG_LOG(SG_INFO, "[CFG] VO hold = 1000 ms");
  radar_send(SET_VO_HOLD_1S, sizeof(SET_VO_HOLD_1S));

  SG_LOG(SG_INFO, "[CFG] Presence max = 200 cm");
  radar_send(SET_PRES_MAX_200CM, sizeof(SET_PRES_MAX_200CM));

  SG_LOG(SG_INFO, "[CFG] Save all");
  radar_send(SAVE_ALL, sizeof(SAVE_ALL));

  g_after_cfg_ms = millis() + 1000; // 1 segundo de warm-up
}

// === Telemetria ===
static RadarParsed g_last;
static bool g_has_last = false;
static uint32_t g_last_seen_ms = 0;
static uint32_t g_tick_ms = 0;
static const uint32_t TICK_PERIOD_MS = 250;
static const uint32_t STALE_MS = 3000;

// streaming throttle
static uint32_t g_last_stream_ms = 0;

static void print_measurement(const RadarParsed& p, uint32_t t_ms) {
  if (sg_cli_json_on()) {
    Serial.print('{');
    Serial.print(F("\"ts_ms\":"));    Serial.print(t_ms); Serial.print(',');
    Serial.print(F("\"status\":\""));  Serial.print(status_str(p.status)); Serial.print(F("\","));
    Serial.print(F("\"dist_m\":"));    Serial.print(p.dist_m, 3);   Serial.print(',');
    Serial.print(F("\"speed_mps\":")); Serial.print(p.speed_mps, 3);Serial.print(',');
    Serial.print(F("\"snr\":"));       Serial.print(p.snr, 3);      Serial.print(',');
    Serial.print(F("\"distance_cm\":")); Serial.print(p.distance_cm); Serial.print(',');
    Serial.print(F("\"speed_cms\":"));   Serial.print(p.speed_cms);   Serial.print(',');
    Serial.print(F("\"signal\":"));      Serial.print(p.signal);
    Serial.println('}');
  } else {
    Serial.print(F("ts=")); Serial.print(t_ms);
    Serial.print(F(" status=")); Serial.print(status_str(p.status));
    Serial.print(F(" dist_cm=")); Serial.print(p.distance_cm);
    Serial.print(F(" speed_cms=")); Serial.print(p.speed_cms);
    Serial.print(F(" signal=")); Serial.println(p.signal);
  }
}

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 1500) { /* aguarda enumerar */ }

  sg_cli_init(&Serial);
  sg_log_set_level(SG_INFO);

  pinMode(SG_PIN_RADAR_OCC, INPUT_PULLDOWN);

  if (!uart_begin(&g_uart, /*uart_num*/1, SG_RADAR_BAUD, /*RX*/SG_RADAR_RX, /*TX*/SG_RADAR_TX)) {
    SG_LOG(SG_ERROR, "[ERR] uart_begin falhou");
  }
  if (!radar_begin(&g_radar, &g_uart)) {
    SG_LOG(SG_ERROR, "[ERR] radar_begin falhou");
  }

  ring_init(&g_ring, g_samples, 256);

  SG_LOG(SG_INFO, "[BOOT] SenseGrid CLI/diag (VO=1s, MAX=200cm)");
  Serial.print(F("[UART1] RX=")); Serial.print(SG_RADAR_RX);
  Serial.print(F(" TX=")); Serial.print(SG_RADAR_TX);
  Serial.print(F(" @"));    Serial.println(SG_RADAR_BAUD);
  Serial.print(F("[OCC] pin=")); Serial.println(SG_PIN_RADAR_OCC);

  radar_config_boot();
  SG_LOG(SG_INFO, "type 'help' for commands");
}

void loop() {
  // CLI
  sg_cli_poll();

  // 1) OCC digital
  int occ = digitalRead(SG_PIN_RADAR_OCC);
  if (occ != last_occ) {
    last_occ = occ;
    SG_LOG(SG_INFO, "[OCC] %d  t=%lu", occ, (unsigned long)millis());
  }

  // 2) leitura (até 3 frames por loop)
  if (millis() >= g_after_cfg_ms) {
    for (int i = 0; i < 3; ++i) {
      RadarParsed p;
      if (!radar_read_parsed(&g_radar, &p, /*timeout_ms*/20)) break;
      g_last = p;
      g_has_last = true;
      g_last_seen_ms = millis();

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

      // stream control (JSON/text + rate throttle)
      if (sg_cli_stream_on()) {
        uint16_t min_dt = sg_cli_rate_ms();
        if (min_dt == 0 || (millis() - g_last_stream_ms) >= min_dt) {
          print_measurement(p, g_last_seen_ms);
          g_last_stream_ms = millis();
        }
      }
    }
  }

  // 3) Ticker — somente se não estiver streamando, para evitar spam
  if (!sg_cli_stream_on() && (millis() - g_tick_ms >= TICK_PERIOD_MS)) {
    g_tick_ms = millis();
    if (g_has_last) {
      bool stale = (millis() - g_last_seen_ms > STALE_MS);
      uint8_t shown;
      if (stale) {
        shown = (digitalRead(SG_PIN_RADAR_OCC) == 1) ? 2 /*exist*/ : 0 /*none*/;
      } else {
        shown = g_last.status;
      }
      SG_LOG(SG_DEBUG, "[TICK] %s dist=%ucm signal=%u t=%lu",
            status_str(shown), (unsigned)g_last.distance_cm, (unsigned)g_last.signal, (unsigned long)millis());
    } else {
      SG_LOG(SG_DEBUG, "[TICK] aguardando primeira leitura...");
    }
  }

  // 4) Dreno ring (placeholder)
  SgSample out;
  if (ring_pop(&g_ring, &out)) { /* noop */ }

  delay(2);
}
