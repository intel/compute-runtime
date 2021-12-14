/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_configuration/aub_tests/aub_tests_configuration.h"
#include "shared/test/common/test_macros/test.h"

#include "hw_cmds.h"
using namespace NEO;

template <>
AubTestsConfig GetAubTestsConfig<XE_HPC_COREFamily>() {
    AubTestsConfig aubTestsConfig;
    aubTestsConfig.testCanonicalAddress = false;
    return aubTestsConfig;
}
