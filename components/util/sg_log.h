#pragma once
#include <Arduino.h>

enum SgLogLevel : uint8_t {
  SG_ERROR = 0,
  SG_WARN  = 1,
  SG_INFO  = 2,
  SG_DEBUG = 3,
};

extern volatile uint8_t g_sg_log_level;

inline const __FlashStringHelper* sg_log_level_name(uint8_t lv) {
  switch (lv) {
    case SG_ERROR: return F("ERROR");
    case SG_WARN:  return F("WARN");
    case SG_INFO:  return F("INFO");
    case SG_DEBUG: return F("DEBUG");
    default:       return F("?");
  }
}

// printf-like macro (ESP32 suporta Serial.printf).
// Usa filtro por nível: só imprime se lv <= g_sg_log_level.
#define SG_LOG(lv, fmt, ...) do {                                 \
  if ((uint8_t)(lv) <= g_sg_log_level) {                           \
    Serial.printf("[%s] ", reinterpret_cast<const char *>(sg_log_level_name(lv))); \
    Serial.printf(fmt, ##__VA_ARGS__);                             \
    Serial.print("\r\n");                                          \
  }                                                                \
} while(0)

inline void sg_log_set_level(uint8_t lv) { g_sg_log_level = lv; }
inline uint8_t sg_log_get_level() { return g_sg_log_level; }
