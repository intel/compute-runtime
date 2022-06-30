/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/test_configuration/aub_tests/aub_tests_configuration.h"

using namespace NEO;

template <>
AubTestsConfig getAubTestsConfig<XeHpFamily>() {
    AubTestsConfig aubTestsConfig;
    aubTestsConfig.testCanonicalAddress = true;
    return aubTestsConfig;
}
