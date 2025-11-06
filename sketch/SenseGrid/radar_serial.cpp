// sketch/SenseGrid/radar_serial.cpp
// Implementação da UART do radar e leitura do pino de ocupação.
#include <Arduino.h>
#include "pins_radar.h"

// Instancia a UART1 para o radar
HardwareSerial Radar(SG_RADAR_UART_NUM);

void radar_serial_begin() {
  // Linha discreta de presença do radar
  pinMode(SG_PIN_RADAR_OCC, INPUT_PULLDOWN);
  // Tempo para o módulo acordar/estabilizar
  delay(400);
  // Mapeia UART1 nos GPIOs escolhidos (RX, TX) e configura baud/paridade
  Radar.begin(SG_RADAR_BAUD, SERIAL_8N1, SG_RADAR_RX, SG_RADAR_TX);
}

int radar_serial_available() {
  return Radar.available();
}

int radar_serial_read_byte() {
  return Radar.read();
}

size_t radar_serial_write_byte(uint8_t b) {
  return Radar.write(b);
}

int radar_occupancy_pin_read() {
  return digitalRead(SG_PIN_RADAR_OCC);
}
