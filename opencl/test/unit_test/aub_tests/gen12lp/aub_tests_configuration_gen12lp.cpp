/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/aub_tests_configuration.h"

#include "hw_cmds.h"

using namespace NEO;

template <>
AubTestsConfig GetAubTestsConfig<TGLLPFamily>() {
    AubTestsConfig aubTestsConfig;
    aubTestsConfig.testCanonicalAddress = false;
    return aubTestsConfig;
}