#pragma once
// Shim header for Arduino build: maps <adapters_mqtt.h> to the IDF header location.
// This allows components/adapters_mqtt/adapters_mqtt.c to #include "adapters_mqtt.h" successfully
// when compiled via Arduino library wrappers.
#include "../../../../components/adapters_mqtt/include/adapters_mqtt.h"
