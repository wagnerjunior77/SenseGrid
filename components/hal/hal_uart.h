
// components/hal/hal_uart.h
// UART wrapper (HAL) — SenseGrid
// Objetivo: oferecer uma API mínima e estável para UART independente de endpoint/protocolo.
// DoD: uart_begin(), uart_read_frame(), uart_write()
//
// Notas:
// - Implementação usa Arduino (HardwareSerial) por baixo, mas a interface não expõe Arduino.
// - uart_read_frame() possui framing genérico: procura header[0..1], lê LEN (2B big-endian),
//   e retorna o frame completo [Hdr2][Len2][Payload(Len bytes)].
// - timeout_ms vale para achar header e para completar o frame.
//
// Erros (retorno negativo):
//  -1: timeout procurando header
//  -2: timeout lendo o restante do frame
//  -3: frame maior que o buffer (overflow)
//
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct UartHandle {
  void* impl;     // ponteiro opaco (HardwareSerial*)
  int   rx_pin;
  int   tx_pin;
  uint32_t baud;
  int   uart_num; // 0/1/2 (no ESP32-C3: usamos 1 para o radar)
} UartHandle;

// Inicializa a UART física.
// Retorna true em caso de sucesso.
bool uart_begin(UartHandle* h, int uart_num, uint32_t baud, int rx_pin, int tx_pin);

// Escreve um bloco de bytes; retorna quantidade escrita.
size_t uart_write(UartHandle* h, const uint8_t* data, size_t len);

// Lê um frame com a seguinte convenção:
//   Header[2] (hdr0,hdr1), Len[2] (big-endian) e Len bytes seguintes (Func+Cmd+Data+Chk, p.ex. no ME73).
// Guarda no buffer: Header(2) + Len(2) + Payload(Len)  => total = 4 + Len
// Retorna total de bytes copiados (>=4) ou código de erro negativo.
int uart_read_frame(UartHandle* h,
                    uint8_t* out_buf,
                    size_t   out_max,
                    uint32_t timeout_ms,
                    uint8_t  hdr0,
                    uint8_t  hdr1);
