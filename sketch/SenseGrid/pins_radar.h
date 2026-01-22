// sketch/SenseGrid/pins_radar.h
// Pinagem e API minima do radar - SenseGrid
// Mantem a UART0 (Serial) para logs e isola o radar na UART1.
#pragma once
#include <Arduino.h>

// ---- Pinout oficial (A2/HW) ----
// Linha discreta de ocupacao do radar (pino 'O' no modulo)
constexpr int SG_PIN_RADAR_OCC = 4;      // Radar.O -> ESP GPIO4

// UART dedicada ao radar (pinos 'T' e 'R' no modulo)
constexpr int SG_RADAR_UART_NUM = 1;     // HardwareSerial(1)
constexpr int SG_RADAR_RX       = 1;     // Radar.T -> ESP RX (GPIO1)
constexpr int SG_RADAR_TX       = 3;     // Radar.R <- ESP TX (GPIO3)
constexpr long SG_RADAR_BAUD    = 115200;
