#include "sg_cli.h"
#include <ctype.h>
#include <string.h>

static SgCliHelpFn   cb_help   = nullptr;
static SgCliInfoFn   cb_info   = nullptr;
static SgCliStreamFn cb_stream = nullptr;
static SgCliRateFn   cb_rate   = nullptr;
static SgCliJsonFn   cb_json   = nullptr;
static SgCliLogFn    cb_log    = nullptr;

void sg_cli_set_handlers(SgCliHelpFn h, SgCliInfoFn i, SgCliStreamFn s,
                         SgCliRateFn r, SgCliJsonFn j, SgCliLogFn l) {
  cb_help   = h;
  cb_info   = i;
  cb_stream = s;
  cb_rate   = r;
  cb_json   = j;
  cb_log    = l;
}

void sg_cli_print_default_help(Print& out) {
  out.println(F("Commands:"));
  out.println(F("  help"));
  out.println(F("  info"));
  out.println(F("  stream on|off"));
  out.println(F("  rate <ms>            (ex.: rate 100)"));
  out.println(F("  json on|off          (liga/desliga JSON de streaming)"));
  out.println(F("  log error|warn|info|debug"));
}

static void trim(char* s) {
  // remove \r\n do fim
  int n = (int)strlen(s);
  while (n > 0 && (s[n-1] == '\r' || s[n-1] == '\n' || isspace((unsigned char)s[n-1]))) {
    s[--n] = 0;
  }
  // começos
  int i = 0;
  while (s[i] && isspace((unsigned char)s[i])) i++;
  if (i > 0) memmove(s, s + i, strlen(s + i) + 1);
}

static int split_tokens(char* line, char* argv[], int maxv) {
  int argc = 0;
  char* p = line;
  while (*p && argc < maxv) {
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) break;
    argv[argc++] = p;
    while (*p && !isspace((unsigned char)*p)) p++;
    if (*p) { *p = 0; p++; }
  }
  return argc;
}

static int level_from_word(const char* w) {
  if (!w) return 2; // info default
  if (!strcasecmp(w, "error")) return 0;
  if (!strcasecmp(w, "warn"))  return 1;
  if (!strcasecmp(w, "info"))  return 2;
  if (!strcasecmp(w, "debug")) return 3;
  return 2;
}

void sg_cli_poll(Stream& in, Print& out) {
  static char  line[128];
  static size_t idx = 0;

  while (in.available() > 0) {
    int ch = in.read();
    if (ch < 0) break;

    if (ch == '\n' || ch == '\r') {
      if (idx == 0) continue; // linha vazia
      line[idx] = 0; idx = 0;

      trim(line);
      if (!line[0]) continue;

      // tokeniza
      char* argv[5] = {0};
      int argc = split_tokens(line, argv, 5);
      const char* cmd = argv[0];

      if (!strcasecmp(cmd, "help")) {
        if (cb_help) cb_help(out); else sg_cli_print_default_help(out);
        continue;
      }

      if (!strcasecmp(cmd, "info")) {
        if (cb_info) cb_info(out); else out.println(F("[WARN] info: handler not set"));
        continue;
      }

      if (!strcasecmp(cmd, "stream") && argc >= 2) {
        bool on = !strcasecmp(argv[1], "on");
        if (cb_stream) cb_stream(on); else out.println(F("[WARN] stream: handler not set"));
        continue;
      }

      if (!strcasecmp(cmd, "rate") && argc >= 2) {
        uint32_t v = (uint32_t)strtoul(argv[1], nullptr, 10);
        if (cb_rate) cb_rate(v); else out.println(F("[WARN] rate: handler not set"));
        continue;
      }

      if (!strcasecmp(cmd, "json") && argc >= 2) {
        bool on = !strcasecmp(argv[1], "on");
        if (cb_json) cb_json(on); else out.println(F("[WARN] json: handler not set"));
        continue;
      }

      if (!strcasecmp(cmd, "log") && argc >= 2) {
        int lvl = level_from_word(argv[1]);
        if (cb_log) cb_log(lvl); else out.println(F("[WARN] log: handler not set"));
        continue;
      }

      out.print(F("[WARN] unknown: "));
      out.println(cmd);
      sg_cli_print_default_help(out);
    } else {
      if (idx < sizeof(line) - 1) {
        line[idx++] = (char)ch;
      } else {
        // overflow -> reseta
        idx = 0;
      }
    }
  }
}
