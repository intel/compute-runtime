/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/tests_configuration.h"

namespace NEO {
// max time per single test iteration
unsigned int ultIterationMaxTime = 180;
bool useMockGmm = false;
const char *executionDirectorySuffix = "_aub";
TestMode testMode = TestMode::AubTests;
} // namespace NEO
