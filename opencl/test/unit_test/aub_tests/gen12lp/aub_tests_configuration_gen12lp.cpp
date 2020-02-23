/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_cmds.h"
#include "opencl/test/unit_test/aub_tests/aub_tests_configuration.h"

using namespace NEO;

template <>
AubTestsConfig GetAubTestsConfig<TGLLPFamily>() {
    AubTestsConfig aubTestsConfig;
    aubTestsConfig.testCanonicalAddress = false;
    return aubTestsConfig;
}