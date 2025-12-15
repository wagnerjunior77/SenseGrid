#pragma once
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Context for envelopes (shared by HTTP/MQTT)
typedef struct {
  const char* device_id;
  uint32_t    seq;
  uint8_t     v;   // contract version
} SgSerCtx;

typedef struct {
  uint32_t ts_ms;
  int      state;
  float    confidence;
} SgSerOccupancy;

typedef struct {
  uint32_t ts_ms;
  int      count_active;
} SgSerTracks;

typedef struct {
  uint32_t ts_ms;
  uint32_t uptime_s;
  int      rssi_dbm;
} SgSerHealth;

// Message kinds for router helpers
typedef enum {
  SG_SER_TOPIC_MEAS = 0,
  SG_SER_TOPIC_MEAS_RAW,
  SG_SER_TOPIC_EVENTS,
  SG_SER_TOPIC_STATUS,
  SG_SER_TOPIC_CAP,
  SG_SER_TOPIC_ACK,
  SG_SER_TOPIC_ERR,
  SG_SER_TOPIC_DT_META,
  SG_SER_TOPIC_DT_ST,
  SG_SER_TOPIC_DT_OTA,
  SG_SER_TOPIC_DT_CFG_OUT,
  SG_SER_TOPIC_DT_CFG_IN,
  SG_SER_TOPIC_DT_O_OUT,
  SG_SER_TOPIC_DT_O_IN,
  SG_SER_TOPIC_UNKNOWN = 255
} SgSerTopic;

typedef enum {
  SG_SER_PATH_OCCUPANCY = 0,
  SG_SER_PATH_TRACKS,
  SG_SER_PATH_HEALTH,
  SG_SER_PATH_MEAS,
  SG_SER_PATH_CMD,
  SG_SER_PATH_UNKNOWN = 255
} SgSerPath;

void    sg_ser_init(SgSerCtx* ctx, const char* device_id, uint8_t contract_version);
uint32_t sg_ser_next_seq(SgSerCtx* ctx);

// Writes ISO-ish time (UTC, based on ts_ms uptime) into out. Returns bytes written (excluding null).
size_t  sg_ser_ts_iso(uint32_t ts_ms, char* out, size_t out_sz);

// Envelope builder
size_t  sg_ser_envelope(const SgSerCtx* ctx, uint32_t ts_ms, const char* type,
                        const char* payload_json, char* out, size_t out_sz);

// Helpers per payload
size_t  sg_ser_make_occupancy(const SgSerCtx* ctx, const SgSerOccupancy* occ,
                              char* out, size_t out_sz);
size_t  sg_ser_make_tracks(const SgSerCtx* ctx, const SgSerTracks* t,
                           char* out, size_t out_sz);
size_t  sg_ser_make_health(const SgSerCtx* ctx, const SgSerHealth* h,
                           char* out, size_t out_sz);
size_t  sg_ser_make_ack(const SgSerCtx* ctx, uint32_t ts_ms, const char* txid,
                        int ok, char* out, size_t out_sz);
size_t  sg_ser_make_err(const SgSerCtx* ctx, uint32_t ts_ms, const char* txid,
                        const char* code, const char* msg,
                        char* out, size_t out_sz);

// Router helpers
const char* sg_ser_topic(SgSerTopic t);
const char* sg_ser_path(SgSerPath p);

#ifdef __cplusplus
}
#endif
