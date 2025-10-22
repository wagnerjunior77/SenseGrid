#pragma once
// Shim header for Arduino build: maps <drivers_radar.h> to the IDF header location.
// This allows components/drivers_radar/drivers_radar.c to #include "drivers_radar.h" successfully
// when compiled via Arduino library wrappers.
#include "../../../../components/drivers_radar/include/drivers_radar.h"
