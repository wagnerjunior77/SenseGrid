
// components/common/ring_samples.cpp
#include "ring_samples.h"
#include <Arduino.h> // para noInterrupts()/interrupts()

static inline uint16_t mask_of_cap(uint16_t cap) {
  return (uint16_t)(cap - 1);
}

void ring_init(SgRing* r, SgSample* backing, uint16_t capacity_pow2) {
  r->buf = backing;
  r->cap = capacity_pow2;
  r->head = 0;
  r->tail = 0;
}

bool ring_full(const SgRing* r) {
  // full quando (head+1) == tail com wrap
  uint16_t mask = mask_of_cap(r->cap);
  uint16_t next = (r->head + 1) & mask;
  return next == r->tail;
}

bool ring_empty(const SgRing* r) {
  return r->head == r->tail;
}

bool ring_push(SgRing* r, const SgSample* s) {
  uint16_t mask = mask_of_cap(r->cap);
  noInterrupts();
  uint16_t head = r->head;
  uint16_t tail = r->tail;
  uint16_t next = (head + 1) & mask;
  if (next == tail) {
    interrupts();
    return false; // cheio
  }
  r->buf[head] = *s;
  r->head = next;
  interrupts();
  return true;
}

bool ring_pop(SgRing* r, SgSample* out) {
  noInterrupts();
  if (r->head == r->tail) {
    interrupts();
    return false;
  }
  uint16_t tail = r->tail;
  *out = r->buf[tail];
  r->tail = (uint16_t)((tail + 1) & (r->cap - 1));
  interrupts();
  return true;
}

uint16_t ring_size(const SgRing* r) {
  noInterrupts();
  uint16_t head = r->head;
  uint16_t tail = r->tail;
  interrupts();
  uint16_t mask = mask_of_cap(r->cap);
  return (head - tail) & mask;
}
