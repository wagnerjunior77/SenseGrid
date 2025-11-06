
// sketch/SenseGrid/SenseGrid.ino (Tuned cadence)
// Ajustes:
//  - Timeout de leitura por byte: 20 ms (melhor resiliência a jitter)
//  - Até 3 frames por loop
//  - Ticker: se ficar "stale" mas OCC=1, mostra "exist" (fallback de presença)
//  - Warm-up pós-config reduzido para 1 s

#include <Arduino.h>
#include "pins_radar.h"
#include "glue/hal_uart_glue.h"
#include "glue/drv_radar_me73_glue.h"
#include "glue/ring_samples_glue.h"

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
  // VO hold = 3000 ms
  static const uint8_t SET_VO_HOLD_3S[] = {0x55,0x5A,0x00,0x06,0x01,0x80,0x14,0x0B,0xB8,0x0D};
  // Presence max = 300 cm (0x012C)
  static const uint8_t SET_PRESENCE_MAX_300CM[] = {0x55,0x5A,0x00,0x06,0x01,0x80,0x0E,0x01,0x2C,0x71};
  // Save all
  static const uint8_t SAVE_ALL[] = {0x55,0x5A,0x00,0x04,0x01,0x20,0x04,0xD8};

  Serial.println(F("[CFG] VO hold = 3000 ms"));
  radar_send(SET_VO_HOLD_3S, sizeof(SET_VO_HOLD_3S));

  Serial.println(F("[CFG] Presence max = 300 cm"));
  radar_send(SET_PRESENCE_MAX_300CM, sizeof(SET_PRESENCE_MAX_300CM));

  Serial.println(F("[CFG] Save all"));
  radar_send(SAVE_ALL, sizeof(SAVE_ALL));

  g_after_cfg_ms = millis() + 1000; // 1 segundo
}

// === Telemetria ===
static RadarParsed g_last;
static bool g_has_last = false;
static uint32_t g_last_seen_ms = 0;
static uint32_t g_tick_ms = 0;
static const uint32_t TICK_PERIOD_MS = 250;
static const uint32_t STALE_MS = 3000;

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 1500) { /* aguarda enumerar */ }

  pinMode(SG_PIN_RADAR_OCC, INPUT_PULLDOWN);

  if (!uart_begin(&g_uart, /*uart_num*/1, SG_RADAR_BAUD, /*RX*/SG_RADAR_RX, /*TX*/SG_RADAR_TX)) {
    Serial.println(F("[ERR] uart_begin falhou"));
  }
  if (!radar_begin(&g_radar, &g_uart)) {
    Serial.println(F("[ERR] radar_begin falhou"));
  }

  ring_init(&g_ring, g_samples, 256);

  Serial.println(F("\n[BOOT] SenseGrid tuned"));
  Serial.print(F("[UART1] RX=")); Serial.print(SG_RADAR_RX);
  Serial.print(F(" TX=")); Serial.print(SG_RADAR_TX);
  Serial.print(F(" @"));    Serial.println(SG_RADAR_BAUD);
  Serial.print(F("[OCC] pin=")); Serial.println(SG_PIN_RADAR_OCC);

  radar_config_boot();
}

void loop() {
  // 1) OCC digital
  int occ = digitalRead(SG_PIN_RADAR_OCC);
  if (occ != last_occ) {
    last_occ = occ;
    Serial.print(F("[OCC] ")); Serial.print(occ);
    Serial.print(F("  t=")); Serial.println(millis());
  }

  // 2) pular leitura durante warm-up
  if (millis() >= g_after_cfg_ms) {
    // Tenta até 3 frames por ciclo, timeout 20ms
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

      Serial.print(F("[PARSED] status="));
      Serial.print(status_str(p.status));
      Serial.print(F(" dist="));   Serial.print(p.distance_cm);
      Serial.print(F("cm signal=")); Serial.print(p.signal);
      Serial.print(F("  t=")); Serial.println(s.t_ms);
    }
  }

  // 3) Ticker — se stale e OCC=1, mostrar "exist"
  if (millis() - g_tick_ms >= TICK_PERIOD_MS) {
    g_tick_ms = millis();
    if (g_has_last) {
      bool stale = (millis() - g_last_seen_ms > STALE_MS);
      uint8_t shown;
      if (stale) {
        shown = (digitalRead(SG_PIN_RADAR_OCC) == 1) ? 2 /*exist*/ : 0 /*none*/;
      } else {
        shown = g_last.status;
      }
      Serial.print(F("[TICK] status=")); Serial.print(status_str(shown));
      Serial.print(F(" dist="));   Serial.print(g_last.distance_cm);
      Serial.print(F("cm signal=")); Serial.print(g_last.signal);
      Serial.print(F("  t=")); Serial.println(millis());
    } else {
      Serial.println(F("[TICK] aguardando primeira leitura..."));
    }
  }

  // 4) Dreno ring (placeholder)
  SgSample out;
  if (ring_pop(&g_ring, &out)) { /* noop */ }

  delay(2);
}
