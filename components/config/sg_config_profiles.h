#pragma once
#include <stdbool.h>
#include "../pipeline/sg_pipe.h"

#ifdef __cplusplus
extern "C" {
#endif

// Persistent calibration profiles (stored under "calibpf").
bool sg_config_profile_name_ok(const char* name);
bool sg_config_profile_save(const char* name, const SgParams* params);
bool sg_config_profile_load(const char* name, SgParams* out);

#ifdef __cplusplus
}
#endif
