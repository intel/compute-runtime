/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_mode.h"

namespace NEO {
// max time per single test iteration
unsigned int ultIterationMaxTime = 180;
unsigned int testCaseMaxTimeInMs = 20000;
bool useMockGmm = false;
const char *executionDirectorySuffix = "_aub";
const char *executionName = "AUB";
TestMode testMode = defaultTestMode;
} // namespace NEO
