#pragma once
#include <stddef.h>
#include <stdint.h>
#include "../../serializer/sg_serializer.h"

// Alias para manter compatibilidade com chamadas existentes
typedef SgSerCtx        SgHttpCtx;
typedef SgSerOccupancy  SgHttpOccupancy;
typedef SgSerTracks     SgHttpTracks;
typedef SgSerHealth     SgHttpHealth;

// Inicializa contexto (seq inicia em 1)
void sg_http_init(SgHttpCtx* ctx, const char* device_id, uint8_t contract_version);

// Incrementa seq e retorna o novo valor
uint32_t sg_http_next_seq(SgHttpCtx* ctx);

// Monta envelope generico: escreve em buf (JSON) e retorna bytes escritos.
size_t sg_http_envelope(const SgHttpCtx* ctx, uint32_t ts_ms, const char* type,
                        const char* payload_json, char* out, size_t out_sz);

// Helpers para cada endpoint
size_t sg_http_make_occupancy(const SgHttpCtx* ctx, const SgHttpOccupancy* occ,
                              char* out, size_t out_sz);
size_t sg_http_make_tracks(const SgHttpCtx* ctx, const SgHttpTracks* t,
                           char* out, size_t out_sz);
size_t sg_http_make_health(const SgHttpCtx* ctx, const SgHttpHealth* h,
                           char* out, size_t out_sz);
size_t sg_http_make_ack(const SgHttpCtx* ctx, uint32_t ts_ms, const char* txid,
                        int ok, char* out, size_t out_sz);
size_t sg_http_make_err(const SgHttpCtx* ctx, uint32_t ts_ms, const char* txid,
                        const char* code, const char* msg,
                        char* out, size_t out_sz);
