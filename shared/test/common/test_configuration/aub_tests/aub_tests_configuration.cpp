/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_mode.h"

bool threadsAllowed = false;

namespace NEO {
// max time per single test iteration
unsigned int ultIterationMaxTimeInS = 180;
unsigned int testCaseMaxTimeInMs = 32000;
bool useMockGmm = false;
const char *executionDirectorySuffix = "_aub";
const char *executionName = "AUB";
TestMode testMode = defaultTestMode;
} // namespace NEO
