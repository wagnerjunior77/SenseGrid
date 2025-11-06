// sketch/SenseGrid/pins_radar.h
// Pinagem e API mínima de UART do radar — SenseGrid
// Mantém a UART0 (Serial) para logs e isola o radar na UART1.
#pragma once
#include <Arduino.h>

// ---- Pinout oficial (A2/HW) ----
// Linha discreta de ocupação do radar (pino 'O' no módulo)
constexpr int SG_PIN_RADAR_OCC = 4;      // Radar.O -> ESP GPIO4

// UART dedicada ao radar (pinos 'T' e 'R' no módulo)
constexpr int SG_RADAR_UART_NUM = 1;     // HardwareSerial(1)
constexpr int SG_RADAR_RX       = 1;     // Radar.T -> ESP RX (GPIO1)
constexpr int SG_RADAR_TX       = 3;     // Radar.R <- ESP TX (GPIO3)
constexpr long SG_RADAR_BAUD    = 115200;

// Instância global da UART do radar (definida em radar_serial.cpp)
extern HardwareSerial Radar;

// ---- API mínima para a ponte Arduino <-/-> driver C ----
void   radar_serial_begin();                 // inicializa UART1 e o pino OCC
int    radar_serial_available();             // bytes disponíveis para leitura
int    radar_serial_read_byte();             // lê 1 byte
size_t radar_serial_write_byte(uint8_t b);   // escreve 1 byte
int    radar_occupancy_pin_read();           // lê nível lógico do pino OCC
