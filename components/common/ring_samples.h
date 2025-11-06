
// components/common/ring_samples.h
// Ring buffer de amostras com timestamp (millis/us) — thread/ISR-safe
// DoD: capacidade >= 256, API segura no loop (usa seções críticas curtas)
#pragma once
#include <stdint.h>
#include <stddef.h>

struct SgSample {
  uint32_t t_ms;
  uint32_t t_us;
  uint16_t distance_cm;
  int16_t  speed_cms;
  uint16_t signal;
  uint8_t  status;
  uint8_t  _rsv;
};

struct SgRing {
  SgSample* buf;
  uint16_t  cap;     // capacidade (ex.: 256)
  volatile uint16_t head; // posição de escrita
  volatile uint16_t tail; // posição de leitura
};

// Inicializa o ring com o buffer fornecido (cap deve ser potência de 2 para wrap rápido).
void ring_init(SgRing* r, SgSample* backing, uint16_t capacity_pow2);

// Empurra 1 amostra; retorna false se cheio (descarta).
bool ring_push(SgRing* r, const SgSample* s);

// Puxa 1 amostra; retorna false se vazio.
bool ring_pop(SgRing* r, SgSample* out);

// Quantidade de itens no buffer.
uint16_t ring_size(const SgRing* r);

// Cheio/vazio
bool ring_empty(const SgRing* r);
bool ring_full(const SgRing* r);
