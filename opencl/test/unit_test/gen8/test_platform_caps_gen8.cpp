/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

using Gen8PlatformCaps = Test<PlatformFixture>;

GEN8TEST_F(Gen8PlatformCaps, WhenCheckingExtensionStringThenFp64IsSupported) {
    const auto &caps = pPlatform->getPlatformInfo();

    EXPECT_NE(std::string::npos, caps.extensions.find(std::string("cl_khr_fp64")));
}
