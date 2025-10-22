#pragma once
// Shim header for Arduino build: maps <common.h> to the IDF header location.
// This allows components/common/common.c to #include "common.h" successfully
// when compiled via Arduino library wrappers.
#include "../../../../components/common/include/common.h"
