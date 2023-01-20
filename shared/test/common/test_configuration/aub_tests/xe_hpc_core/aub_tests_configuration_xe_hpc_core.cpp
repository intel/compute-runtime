/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/test/common/test_configuration/aub_tests/aub_tests_configuration.h"
#include "shared/test/common/test_macros/test.h"
using namespace NEO;

template <>
AubTestsConfig getAubTestsConfig<XeHpcCoreFamily>() {
    AubTestsConfig aubTestsConfig;
    aubTestsConfig.testCanonicalAddress = false;
    return aubTestsConfig;
}
