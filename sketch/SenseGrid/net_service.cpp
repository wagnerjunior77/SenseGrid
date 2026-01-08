#include "net_service.h"
#include <string.h>

static SgNetInfo* g_info = nullptr;

void net_service_init(SgNetInfo* info) {
  g_info = info;
  sg_net_init(info);
}

void net_service_refresh(SgNetInfo* info) {
  sg_net_get_info(info);
}

void net_service_apply() {
  sg_net_init(g_info);
}

bool net_service_has_sta_credentials() {
  return sg_net_has_sta_credentials();
}

bool net_service_has_ap_credentials() {
  return sg_net_has_ap_credentials();
}

static void print_wifi_status(Print& out) {
  SgNetInfo info{};
  sg_net_get_info(&info);
  bool sta_set = sg_net_has_sta_credentials();
  bool ap_set = sg_net_has_ap_credentials();
  String sta_ip = info.sta_ip.toString();
  String ap_ip = info.ap_ip.toString();
  out.printf("[wifi] sta_set=%u ap_set=%u sta_connected=%u\n",
             (unsigned)sta_set, (unsigned)ap_set, (unsigned)info.sta_connected);
  out.printf("[wifi] sta_ip=%s ap_ip=%s\n", sta_ip.c_str(), ap_ip.c_str());
  if (info.ssid_sta[0]) out.printf("[wifi] sta_ssid=%s\n", info.ssid_sta);
  if (info.ssid_ap[0]) out.printf("[wifi] ap_ssid=%s\n", info.ssid_ap);
}

void net_service_cli(int argc, char* argv[], Print& out) {
  if (argc < 2) {
    out.println(F("[wifi] uso: wifi show | set <ssid> <pass> | ap <ssid> [pass] | clear [sta|ap|all] | apply"));
    return;
  }
  const char* sub = argv[1];
  if (!strcasecmp(sub, "show")) {
    print_wifi_status(out);
    return;
  }
  if (!strcasecmp(sub, "set")) {
    if (argc < 4) {
      out.println(F("[wifi] uso: wifi set <ssid> <pass>"));
      return;
    }
    if (!sg_net_set_sta_credentials(argv[2], argv[3])) {
      out.println(F("[wifi] ssid invalido"));
      return;
    }
    net_service_apply();
    print_wifi_status(out);
    return;
  }
  if (!strcasecmp(sub, "ap")) {
    if (argc < 3) {
      out.println(F("[wifi] uso: wifi ap <ssid> [pass]"));
      return;
    }
    const char* pass = (argc >= 4) ? argv[3] : "";
    if (!sg_net_set_ap_credentials(argv[2], pass)) {
      out.println(F("[wifi] ssid invalido"));
      return;
    }
    net_service_apply();
    print_wifi_status(out);
    return;
  }
  if (!strcasecmp(sub, "clear")) {
    bool clear_sta = true;
    bool clear_ap = true;
    if (argc >= 3) {
      if (!strcasecmp(argv[2], "sta")) {
        clear_ap = false;
      } else if (!strcasecmp(argv[2], "ap")) {
        clear_sta = false;
      } else if (strcasecmp(argv[2], "all")) {
        out.println(F("[wifi] clear: use sta|ap|all"));
        return;
      }
    }
    if (clear_sta) sg_net_clear_sta_credentials();
    if (clear_ap) sg_net_clear_ap_credentials();
    net_service_apply();
    print_wifi_status(out);
    return;
  }
  if (!strcasecmp(sub, "apply")) {
    net_service_apply();
    print_wifi_status(out);
    return;
  }
  out.println(F("[wifi] comando invalido"));
}
