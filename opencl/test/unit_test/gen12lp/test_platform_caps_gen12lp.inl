/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

struct Gen12LpPlatformCaps : public PlatformFixture, public ::testing::Test {
    void SetUp() override {
        PlatformFixture::SetUp();
    }

    void TearDown() override {
        PlatformFixture::TearDown();
    }
};

HWTEST2_F(Gen12LpPlatformCaps, WhenCheckingExtensionStringThenFp64IsNotSupported, IsTGLLP) {
    const auto &caps = pPlatform->getPlatformInfo();

    EXPECT_EQ(std::string::npos, caps.extensions.find(std::string("cl_khr_fp64")));
}
