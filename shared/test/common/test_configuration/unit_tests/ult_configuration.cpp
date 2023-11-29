/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_mode.h"

namespace NEO {
#if defined(_WIN32) && defined(_DEBUG)
unsigned int ultIterationMaxTime = 110;
#else
unsigned int ultIterationMaxTime = 45;
#endif
unsigned int testCaseMaxTimeInMs = 16000;
bool useMockGmm = true;
const char *executionDirectorySuffix = "";
const char *executionName = "ULT";
TestMode testMode = defaultTestMode;
} // namespace NEO
