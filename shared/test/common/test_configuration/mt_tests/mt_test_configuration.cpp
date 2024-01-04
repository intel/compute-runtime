/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_mode.h"

namespace NEO {
#if defined(_WIN32)
unsigned int ultIterationMaxTimeInS = 120;
#else
unsigned int ultIterationMaxTimeInS = 45;
#endif
unsigned int testCaseMaxTimeInMs = 16000;
bool useMockGmm = true;
const char *executionDirectorySuffix = "";
const char *executionName = "MT";
TestMode testMode = defaultTestMode;
} // namespace NEO
