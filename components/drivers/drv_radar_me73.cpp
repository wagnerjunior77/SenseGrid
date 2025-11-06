
// components/drivers/drv_radar_me73.cpp
#include "drv_radar_me73.h"
#include <string.h>

static uint8_t checksum_sum8(const uint8_t* p, size_t n) {
  uint32_t s = 0;
  for (size_t i = 0; i < n; ++i) s += p[i];
  return (uint8_t)(s & 0xFF);
}

bool radar_begin(RadarHandle* r, UartHandle* uart) {
  if (!r || !uart) return false;
  r->uart = uart;
  return true;
}

bool radar_read_raw(RadarHandle* r, RadarRawFrame* out, uint32_t timeout_ms) {
  if (!r || !r->uart || !out) return false;
  int n = uart_read_frame(r->uart, out->data, sizeof(out->data), timeout_ms, 0x55, 0xA5);
  if (n < 0) return false;
  out->size = (uint16_t)n;
  return true;
}

static uint16_t be16(const uint8_t* p) {
  return (uint16_t)((p[0] << 8) | p[1]);
}

bool radar_read_parsed(RadarHandle* r, RadarParsed* out, uint32_t timeout_ms) {
  if (!r || !out) return false;
  RadarRawFrame rf;
  if (!radar_read_raw(r, &rf, timeout_ms)) return false;
  if (rf.size < 4) return false;

  const uint8_t* buf = rf.data;
  uint16_t len = be16(&buf[2]);
  if (rf.size != (size_t)(4 + len)) {
    // tamanho inconsistente
    return false;
  }
  if (len < 4) {
    // precisa ao menos Func + Cmd1 + Cmd2 + SUM
    return false;
  }

  const uint8_t func = buf[4 + 0];
  const uint8_t cmd1 = buf[4 + 1];
  const uint8_t cmd2 = buf[4 + 2];
  const uint8_t* payload = &buf[4 + 3];
  size_t payload_len = len - 4; // exclui SUM (último byte)

  // Confere checksum
  uint8_t expect_sum = buf[4 + len - 1]; // último byte após payload
  uint8_t calc_sum = 0;
  // soma Func, Cmd1, Cmd2, Payload...
  {
    uint32_t s = func + cmd1 + cmd2;
    for (size_t i = 0; i < payload_len - 1; ++i) s += payload[i]; // -1 porque último é SUM
    calc_sum = (uint8_t)(s & 0xFF);
  }
  if (calc_sum != expect_sum) {
    // checksum inválido — considerar como frame descartado
    return false;
  }

  // Preenche campos mínimos conhecidos
  memset(out, 0, sizeof(*out));
  out->func = func;
  out->cmd1 = cmd1;
  out->cmd2 = cmd2;

  // Heurística simples para relatório ativo (func=0x03).
  // Datasheet típico indica:
  //   Data[?] target_id, status, dist(2B), speed(2B), ... signal(2B)
  // Como o layout pode variar por firmware, mantemos STUB tolerante.
  if (func == 0x03 && payload_len >= 10) {
    // Tentativa: [00 01 00 5E 00 00 00 00 01 78] do exemplo
    // mapear: idx 0..9
    // status @ idx 1; dist @ idx 2..3; signal @ idx 8..9
    out->target_id = payload[0];
    out->status = payload[1];
    out->distance_cm = be16(&payload[2]);
    out->speed_cms = (int16_t)be16(&payload[4]); // pode ser 0 se parado
    out->pitch_deg = (int8_t)payload[7];         // placeholder (ajuste quando confirmar layout)
    out->signal = be16(&payload[8]);
    return true;
  }

  // Se não reconhecido, retorna false para forçar tratamento upstream.
  return false;
}
