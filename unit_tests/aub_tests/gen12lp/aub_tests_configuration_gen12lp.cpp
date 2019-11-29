/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_cmds.h"
#include "unit_tests/aub_tests/aub_tests_configuration.h"

using namespace NEO;

template <>
AubTestsConfig GetAubTestsConfig<TGLLPFamily>() {
    AubTestsConfig aubTestsConfig;
    aubTestsConfig.testCanonicalAddress = false;
    return aubTestsConfig;
}