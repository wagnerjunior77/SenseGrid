#pragma once
// Shim header for Arduino build: maps <adapters_http.h> to the IDF header location.
// This allows components/adapters_http/adapters_http.c to #include "adapters_http.h" successfully
// when compiled via Arduino library wrappers.
#include "../../../../components/adapters_http/include/adapters_http.h"
