/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test_mode.h"

namespace NEO {
// max time per single test iteration
unsigned int ultIterationMaxTime = 180;
bool useMockGmm = false;
const char *executionDirectorySuffix = "_aub";
TestMode testMode = defaultTestMode;
} // namespace NEO
