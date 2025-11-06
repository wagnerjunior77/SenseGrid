
// sketch/SenseGrid/main_demo_hal.ino
// SenseGrid — Demo com HAL UART + Driver ME73 + Ring Buffer
// - UART0 (Serial): logs/CLI
// - UART1: radar (RX=GPIO1, TX=GPIO3)
// - Pino OCC (digital): GPIO4
//
// Build: usar as Tasks Arduino (portable). Os .cpp de components/ são puxados via glue/*.cpp

#include <Arduino.h>
#include "pins_radar.h"                    // SG_RADAR_RX/TX/BAUD e SG_PIN_RADAR_OCC
#include "glue/hal_uart_glue.h"            // UartHandle + uart_begin/read_frame/write
#include "glue/drv_radar_me73_glue.h"      // RadarHandle + radar_* API
#include "glue/ring_samples_glue.h"        // SgRing + ring_*

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

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && (millis() - t0) < 1500) { /* aguarda enumerar */ }

  // Inicializa OCC como entrada (linha discreta do radar)
  pinMode(SG_PIN_RADAR_OCC, INPUT_PULLDOWN);

  // UART1 para o radar
  if (!uart_begin(&g_uart, /*uart_num*/1, SG_RADAR_BAUD, /*RX*/SG_RADAR_RX, /*TX*/SG_RADAR_TX)) {
    Serial.println(F("[ERR] uart_begin falhou"));
  }
  if (!radar_begin(&g_radar, &g_uart)) {
    Serial.println(F("[ERR] radar_begin falhou"));
  }

  ring_init(&g_ring, g_samples, 256);

  Serial.println(F("\n[BOOT] SenseGrid main_demo_hal"));
  Serial.print(F("[UART1] RX=")); Serial.print(SG_RADAR_RX);
  Serial.print(F(" TX=")); Serial.print(SG_RADAR_TX);
  Serial.print(F(" @"));    Serial.println(SG_RADAR_BAUD);
  Serial.print(F("[OCC] pin=")); Serial.println(SG_PIN_RADAR_OCC);
}

void loop() {
  // 1) Monitorar OCC (digital)
  int occ = digitalRead(SG_PIN_RADAR_OCC);
  if (occ != last_occ) {
    last_occ = occ;
    Serial.print(F("[OCC] ")); Serial.print(occ);
    Serial.print(F("  t=")); Serial.println(millis());
  }

  // 2) Tentar ler 1 frame já parseado do radar
  RadarParsed p;
  if (radar_read_parsed(&g_radar, &p, /*timeout_ms*/20)) {
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

    Serial.print(F("[PARSED] status="));
    Serial.print(status_str(p.status));
    Serial.print(F(" dist="));
    Serial.print(p.distance_cm);
    Serial.print(F("cm speed="));
    Serial.print(p.speed_cms);
    Serial.print(F("cm/s signal="));
    Serial.print(p.signal);
    Serial.print(F("  t="));
    Serial.println(s.t_ms);
  }

  // 3) Consumidor de ring (exemplo: drena 1 amostra por ciclo se houver)
  SgSample out;
  if (ring_pop(&g_ring, &out)) {
    // Aqui você pode publicar via Logger/MQTT/HTTP (Adapter), salvar em flash, etc.
    // Vamos só sinalizar que drenou:
    // Serial.println(F("[RING] drenou 1 amostra"));
  }

  delay(2);
}
