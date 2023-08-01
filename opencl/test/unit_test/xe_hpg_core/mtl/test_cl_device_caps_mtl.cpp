/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/source/platform/platform_info.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

using MtlPlatformCaps = Test<PlatformFixture>;

MTLTEST_F(MtlPlatformCaps, givenSkuWhenCheckingExtensionThenFp64IsReported) {
    const auto &caps = pPlatform->getPlatformInfo();

    EXPECT_NE(std::string::npos, caps.extensions.find(std::string("cl_khr_fp64")));
}
