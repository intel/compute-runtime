/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds.h"
#include "shared/test/common/test_configuration/aub_tests/aub_tests_configuration.h"
#include "shared/test/common/test_macros/test.h"
using namespace NEO;

template <>
AubTestsConfig getAubTestsConfig<XE_HPC_COREFamily>() {
    AubTestsConfig aubTestsConfig;
    aubTestsConfig.testCanonicalAddress = false;
    return aubTestsConfig;
}
