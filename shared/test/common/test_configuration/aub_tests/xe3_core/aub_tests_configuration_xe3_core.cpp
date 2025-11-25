/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3_core/hw_cmds_xe3_core.h"
#include "shared/test/common/test_configuration/aub_tests/aub_tests_configuration.h"
#include "shared/test/common/test_macros/test.h"
using namespace NEO;

template <>
AubTestsConfig getAubTestsConfig<Xe3CoreFamily>() {
    AubTestsConfig aubTestsConfig;
    aubTestsConfig.testCanonicalAddress = false;
    return aubTestsConfig;
}
