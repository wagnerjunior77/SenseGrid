// SenseGrid.ino — usando parser novo (CMD1=0x81) + JSON

#define SG_JSON_STREAM 1

#include <Arduino.h>
#include "pins_radar.h"
#include "glue/hal_uart_glue.h"
#include "glue/ring_samples_glue.h"
#include "glue/drv_radar_me73_glue.h"   
#include "../../components/util/diag_json.h"           

UartHandle  g_uart;
RadarHandle g_radar;

static SgSample g_samples[256];
SgRing g_ring;

static int last_occ = -1;

void radar_config_boot();

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 1500) {}

  pinMode(SG_PIN_RADAR_OCC, INPUT_PULLDOWN);

  // UART1 exclusiva pro radar
  uart_begin(&g_uart, 1, SG_RADAR_BAUD, SG_RADAR_RX, SG_RADAR_TX);
  radar_begin(&g_radar, &g_uart);

  ring_init(&g_ring, g_samples, 256);

  Serial.println(F("[BOOT] A2 PARSE + JSON (layout 0x03/0x81)"));
  Serial.print(F("[UART1] RX=")); Serial.print(SG_RADAR_RX);
  Serial.print(F(" TX=")); Serial.print(SG_RADAR_TX);
  Serial.print(F(" @")); Serial.println(SG_RADAR_BAUD);

  radar_config_boot();
  delay(1000);
}

void radar_config_boot() {
  static const uint8_t SET_VO_HOLD_3S[] = {0x55,0x5A,0x00,0x06,0x01,0x80,0x14,0x0B,0xB8,0x0D};
  static const uint8_t SET_PRESENCE_MAX_300CM[] = {0x55,0x5A,0x00,0x06,0x01,0x80,0x0E,0x01,0x2C,0x71};
  static const uint8_t SAVE_ALL[] = {0x55,0x5A,0x00,0x04,0x01,0x20,0x04,0xD8};

  uart_write(&g_uart, SET_VO_HOLD_3S, sizeof(SET_VO_HOLD_3S));
  delay(20);
  uart_write(&g_uart, SET_PRESENCE_MAX_300CM, sizeof(SET_PRESENCE_MAX_300CM));
  delay(20);
  uart_write(&g_uart, SAVE_ALL, sizeof(SAVE_ALL));
  delay(20);
}

static const char* status_str(uint8_t s) {
  switch (s) {
    case 0: return "none";
    case 1: return "move";
    case 2: return "exist";
    default: return "?";
  }
}

void loop() {
  // OCC
  int occ = digitalRead(SG_PIN_RADAR_OCC);
  if (occ != last_occ) {
    last_occ = occ;
    Serial.print(F("[OCC] ")); Serial.print(occ);
    Serial.print(F("  t=")); Serial.println(millis());
  }

  // Radar
  RadarParsed p;
  if (radar_read_parsed(&g_radar, &p, 50)) {
#if SG_JSON_STREAM
    // versão JSON (1 linha)
    Serial.print('{');
    Serial.print("\"ts_ms\":"); Serial.print(millis());
    Serial.print(",\"status\":\""); Serial.print(status_str(p.status)); Serial.print('"');
    Serial.print(",\"dist_m\":"); Serial.print(p.dist_m, 3);
    Serial.print(",\"speed_mps\":"); Serial.print(p.speed_mps, 3);
    Serial.print(",\"snr\":"); Serial.print(p.snr, 3);
    Serial.print(",\"distance_cm\":"); Serial.print(p.distance_cm);
    Serial.print(",\"speed_cms\":"); Serial.print(p.speed_cms);
    Serial.print(",\"signal\":"); Serial.print(p.signal);
    Serial.println('}');
#else
    // versão humana
    Serial.print(F("[PARSED] "));
    Serial.print(status_str(p.status));
    Serial.print(F(" dist=")); Serial.print(p.distance_cm);
    Serial.print(F("cm sig=")); Serial.print(p.signal);
    Serial.print(F(" t=")); Serial.println(millis());
#endif

    // põe no ring
    SgSample s {
      .t_ms = millis(),
      .t_us = micros(),
      .distance_cm = p.distance_cm,
      .speed_cms   = p.speed_cms,
      .signal      = p.signal,
      .status      = p.status,
      ._rsv        = 0,
    };
    ring_push(&g_ring, &s);
  }

  // drena
  SgSample out;
  ring_pop(&g_ring, &out);

  delay(2);
}
