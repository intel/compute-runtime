/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/test/common/test_configuration/aub_tests/aub_tests_configuration.h"

using namespace NEO;

template <>
AubTestsConfig getAubTestsConfig<Gen12LpFamily>() {
    AubTestsConfig aubTestsConfig;
    aubTestsConfig.testCanonicalAddress = false;
    return aubTestsConfig;
}