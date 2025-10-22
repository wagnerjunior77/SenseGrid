#pragma once
// Shim header for Arduino build: maps <drivers_env.h> to the IDF header location.
// This allows components/drivers_env/drivers_env.c to #include "drivers_env.h" successfully
// when compiled via Arduino library wrappers.
#include "../../../../components/drivers_env/include/drivers_env.h"
