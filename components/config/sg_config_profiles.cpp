#include "sg_config_profiles.h"
#include <Preferences.h>
#include <string.h>

static const char* NAMESPACE_PROFILES = "calibpf";

bool sg_config_profile_name_ok(const char* name) {
  if (!name) return false;
  size_t n = strlen(name);
  if (n == 0 || n > 15) return false;
  for (size_t i = 0; i < n; ++i) {
    char c = name[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

bool sg_config_profile_save(const char* name, const SgParams* params) {
  if (!params) return false;
  if (!sg_config_profile_name_ok(name)) return false;
  Preferences pref;
  pref.begin(NAMESPACE_PROFILES, false);
  size_t n = pref.putBytes(name, params, sizeof(SgParams));
  pref.end();
  return n == sizeof(SgParams);
}

bool sg_config_profile_load(const char* name, SgParams* out) {
  if (!out) return false;
  if (!sg_config_profile_name_ok(name)) return false;
  Preferences pref;
  pref.begin(NAMESPACE_PROFILES, true);
  size_t got = pref.getBytes(name, out, sizeof(SgParams));
  pref.end();
  return got == sizeof(SgParams);
}
