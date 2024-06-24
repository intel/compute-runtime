/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_mode.h"

namespace NEO {
#if defined(_WIN32)
#if defined(_DEBUG)
unsigned int ultIterationMaxTimeInS = 240;
#else
unsigned int ultIterationMaxTimeInS = 120;
#endif
#else
#if defined(_DEBUG)
unsigned int ultIterationMaxTimeInS = 90;
#else
unsigned int ultIterationMaxTimeInS = 45;
#endif
#endif
unsigned int testCaseMaxTimeInMs = 16000;
bool useMockGmm = true;
const char *executionDirectorySuffix = "";
const char *executionName = "ULT";
TestMode testMode = defaultTestMode;
} // namespace NEO
