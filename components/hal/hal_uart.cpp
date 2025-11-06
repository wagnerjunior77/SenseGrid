// components/hal/hal_uart.cpp (ESP32-C3 safe)
// Abstração de UART — remove uso de Serial2 (não existe no C3)
#include "hal_uart.h"
#include <Arduino.h>

static HardwareSerial* get_serial_from_num(int uart_num) {
  // ESP32-C3 expõe Serial (USB CDC/JTAG) e Serial1 (UART1).
  // Evitamos Serial2 aqui para compatibilidade com o C3.
  switch (uart_num) {
    case 0: return &Serial;   // console/log — evite para data link
    case 1: return &Serial1;  // recomendado para o radar
    default: return &Serial1; // fallback seguro
  }
}

bool uart_begin(UartHandle* h, int uart_num, uint32_t baud, int rx_pin, int tx_pin) {
  if (!h) return false;
  HardwareSerial* ser = get_serial_from_num(uart_num);
  if (!ser) return false;
  ser->begin(baud, SERIAL_8N1, rx_pin, tx_pin);
  h->impl = ser;
  h->rx_pin = rx_pin;
  h->tx_pin = tx_pin;
  h->baud = baud;
  h->uart_num = uart_num;
  return true;
}

size_t uart_write(UartHandle* h, const uint8_t* data, size_t len) {
  if (!h || !h->impl || !data || !len) return 0;
  HardwareSerial* ser = reinterpret_cast<HardwareSerial*>(h->impl);
  return ser->write(data, len);
}

static bool timed_read(HardwareSerial* ser, uint8_t& out, uint32_t& start_ms, uint32_t timeout_ms) {
  while ((millis() - start_ms) < timeout_ms) {
    if (ser->available() > 0) {
      int v = ser->read();
      if (v >= 0) { out = (uint8_t)v; return true; }
    }
    delay(1);
  }
  return false;
}

int uart_read_frame(UartHandle* h,
                    uint8_t* out_buf,
                    size_t   out_max,
                    uint32_t timeout_ms,
                    uint8_t  hdr0,
                    uint8_t  hdr1) {
  if (!h || !h->impl || !out_buf || out_max < 4) return -3;
  HardwareSerial* ser = reinterpret_cast<HardwareSerial*>(h->impl);
  uint32_t t0 = millis();

  // 1) procurar hdr0
  uint8_t b=0;
  bool ok = false;
  while ((millis() - t0) < timeout_ms) {
    if (ser->available() > 0) {
      int v = ser->read();
      if (v < 0) continue;
      if ((uint8_t)v == hdr0) { ok = true; break; }
    } else {
      delay(1);
    }
  }
  if (!ok) return -1;

  // 2) hdr1
  if (!timed_read(ser, b, t0, timeout_ms)) return -2;
  if (b != hdr1) return -2;

  // 3) LEN (2B BE)
  uint8_t len_hi=0, len_lo=0;
  if (!timed_read(ser, len_hi, t0, timeout_ms)) return -2;
  if (!timed_read(ser, len_lo, t0, timeout_ms)) return -2;
  uint16_t len = ((uint16_t)len_hi << 8) | len_lo;
  size_t total = 4 + (size_t)len;
  if (total > out_max) {
    // drena o restante para manter UART consistente
    for (size_t i = 0; i < len; ++i) { uint8_t dump; if (!timed_read(ser, dump, t0, timeout_ms)) break; }
    return -3;
  }

  // 4) copia para out_buf
  size_t idx = 0;
  out_buf[idx++] = hdr0;
  out_buf[idx++] = hdr1;
  out_buf[idx++] = len_hi;
  out_buf[idx++] = len_lo;

  for (uint16_t i = 0; i < len; ++i) {
    uint8_t d=0;
    if (!timed_read(ser, d, t0, timeout_ms)) return -2;
    out_buf[idx++] = d;
  }
  return (int)idx;
}
