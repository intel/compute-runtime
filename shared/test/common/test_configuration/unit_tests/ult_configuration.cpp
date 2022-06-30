/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_mode.h"

namespace NEO {
unsigned int ultIterationMaxTime = 45;
bool useMockGmm = true;
const char *executionDirectorySuffix = "";
const char *executionName = "ULT";
TestMode testMode = defaultTestMode;
} // namespace NEO