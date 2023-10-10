/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_mode.h"

namespace NEO {
unsigned int ultIterationMaxTime = 45;
unsigned int testCaseMaxTimeInMs = 5000;
bool useMockGmm = true;
const char *executionDirectorySuffix = "";
const char *executionName = "MT";
TestMode testMode = defaultTestMode;
} // namespace NEO
