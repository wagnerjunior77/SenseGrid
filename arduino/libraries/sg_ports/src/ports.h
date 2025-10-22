#pragma once
// Shim header for Arduino build: maps <ports.h> to the IDF header location.
// This allows components/ports/ports.c to #include "ports.h" successfully
// when compiled via Arduino library wrappers.
#include "../../../../components/ports/include/ports.h"
