/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_mode.h"

namespace NEO {
// max time per single test iteration
#if defined(_WIN32)
unsigned int ultIterationMaxTimeInS = 1080;
#else
unsigned int ultIterationMaxTimeInS = 180;
#endif
unsigned int testCaseMaxTimeInMs = 32000;
bool useMockGmm = false;
const char *executionDirectorySuffix = "_aub";
const char *executionName = "AUB";
TestMode testMode = defaultTestMode;
} // namespace NEO
