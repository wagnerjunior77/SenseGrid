#include "sg_cli.h"
#include <ctype.h>

static Stream* s_io = nullptr;
static SgCliState s_state;
static char s_buf[96];
static uint8_t s_len = 0;

static void println(const __FlashStringHelper* s) { if (s_io) { s_io->println(s); } }
static void print(const __FlashStringHelper* s)   { if (s_io) { s_io->print(s); } }
static void println_raw(const char* s) { if (s_io) { s_io->println(s); } }
static void print_raw(const char* s)   { if (s_io) { s_io->print(s); } }

static void show_help() {
  println(F("Commands:"));
  println(F("  help                 - show this help"));
  println(F("  info                 - firmware & runtime info"));
  println(F("  stream on|off        - enable/disable continuous output"));
  println(F("  json on|off          - choose JSON or text format (used by stream)"));
  println(F("  rate <ms>            - min interval between prints (0=no throttle)"));
  println(F("  log <err|warn|info|debug|0..3> - set log level"));
  println(F("  ver                  - show version string"));
}

static void show_info() {
  print(F("Build: ")); println(F(__DATE__ " " __TIME__));
  print(F("Log level: ")); println(sg_log_level_name(sg_log_get_level()));
  print(F("Stream: ")); println(s_state.stream_on ? F("on") : F("off"));
  print(F("JSON: ")); println(s_state.json_on ? F("on") : F("off"));
  print(F("Rate: ")); s_io->println(s_state.rate_ms);
}

static int icmp(const char* a, const char* b) { // case-insensitive strcmp
  while (*a && *b) {
    char ca = tolower(*a++), cb = tolower(*b++);
    if (ca != cb) return (int)ca - (int)cb;
  }
  return (int)(*a) - (int)(*b);
}

static uint8_t level_from_str(const char* s) {
  if (!s) return sg_log_get_level();
  if (isdigit(*s)) return (uint8_t)atoi(s);
  if (!icmp(s, "error") || !icmp(s, "err")) return SG_ERROR;
  if (!icmp(s, "warn"))  return SG_WARN;
  if (!icmp(s, "info"))  return SG_INFO;
  if (!icmp(s, "debug")) return SG_DEBUG;
  return sg_log_get_level();
}

static void handle_line(char* line) {
  // tokeniza (simples)
  char* cmd = strtok(line, " \t\r\n");
  if (!cmd) return;

  if (!icmp(cmd, "help") || !icmp(cmd, "?")) {
    show_help();
    return;
  }
  if (!icmp(cmd, "info")) {
    show_info();
    return;
  }
  if (!icmp(cmd, "ver") || !icmp(cmd, "version")) {
    println(F("SenseGrid A2 CLI/diag"));
    return;
  }
  if (!icmp(cmd, "stream")) {
    char* arg = strtok(nullptr, " \t\r\n");
    if (!arg) { println(F("usage: stream on|off")); return; }
    s_state.stream_on = (!icmp(arg, "on"));
    print(F("stream: ")); println(s_state.stream_on ? F("on") : F("off"));
    return;
  }
  if (!icmp(cmd, "json")) {
    char* arg = strtok(nullptr, " \t\r\n");
    if (!arg) { println(F("usage: json on|off")); return; }
    s_state.json_on = (!icmp(arg, "on"));
    print(F("json: ")); println(s_state.json_on ? F("on") : F("off"));
    return;
  }
  if (!icmp(cmd, "rate")) {
    char* arg = strtok(nullptr, " \t\r\n");
    if (!arg) { println(F("usage: rate <ms> (0 = no throttle)")); return; }
    int v = atoi(arg);
    if (v < 0) v = 0;
    if (v > 60000) v = 60000;
    s_state.rate_ms = (uint16_t)v;
    print(F("rate: ")); s_io->println(s_state.rate_ms);
    return;
  }
  if (!icmp(cmd, "log")) {
    char* arg = strtok(nullptr, " \t\r\n");
    if (!arg) { println(F("usage: log <err|warn|info|debug|0..3>")); return; }
    uint8_t lv = level_from_str(arg);
    if (lv > SG_DEBUG) lv = SG_DEBUG;
    sg_log_set_level(lv);
    print(F("log: ")); println(sg_log_level_name(lv));
    return;
  }

  println(F("unknown command. type 'help'."));
}

void sg_cli_init(Stream* io) {
  s_io = io;
  s_len = 0;
  memset(s_buf, 0, sizeof(s_buf));
}

void sg_cli_poll() {
  if (!s_io) return;
  while (s_io->available() > 0) {
    int c = s_io->read();
    if (c < 0) break;
    char ch = (char)c;
    if (ch == '\r' || ch == '\n') {
      s_buf[s_len] = 0;
      handle_line(s_buf);
      s_len = 0;
      continue;
    }
    if (ch == 0x08 || ch == 0x7F) { // backspace/delete
      if (s_len > 0) s_len--;
      continue;
    }
    if (s_len < (sizeof(s_buf)-1)) {
      s_buf[s_len++] = ch;
    }
  }
}

bool sg_cli_stream_on() { return s_state.stream_on; }
bool sg_cli_json_on()   { return s_state.json_on; }
uint16_t sg_cli_rate_ms(){ return s_state.rate_ms; }
