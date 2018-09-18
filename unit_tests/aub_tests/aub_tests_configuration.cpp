/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/tests_configuration.h"

namespace OCLRT {
// max time per single test iteration
unsigned int ultIterationMaxTime = 180;
bool useMockGmm = false;
const char *executionDirectorySuffix = "_aub";
TestMode testMode = TestMode::AubTests;
} // namespace OCLRT
