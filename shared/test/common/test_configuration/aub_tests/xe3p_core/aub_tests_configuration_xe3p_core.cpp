/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/test_configuration/aub_tests/aub_tests_configuration.h"
#include "shared/test/common/test_macros/test.h"

#include "hw_cmds_xe3p_core.h"
using namespace NEO;

template <>
AubTestsConfig getAubTestsConfig<Xe3pCoreFamily>() {
    AubTestsConfig aubTestsConfig;
    aubTestsConfig.testCanonicalAddress = false;
    return aubTestsConfig;
}
