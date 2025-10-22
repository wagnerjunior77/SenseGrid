#pragma once
// Shim header for Arduino build: maps <parser_radar.h> to the IDF header location.
// This allows components/parser_radar/parser_radar.c to #include "parser_radar.h" successfully
// when compiled via Arduino library wrappers.
#include "../../../../components/parser_radar/include/parser_radar.h"
