#pragma once
// Shim header for Arduino build: maps <adapters_flashrepo.h> to the IDF header location.
// This allows components/adapters_flashrepo/adapters_flashrepo.c to #include "adapters_flashrepo.h" successfully
// when compiled via Arduino library wrappers.
#include "../../../../components/adapters_flashrepo/include/adapters_flashrepo.h"
