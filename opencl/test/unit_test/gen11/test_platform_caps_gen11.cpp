/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

struct Gen11PlatformCaps : public PlatformFixture, public ::testing::Test {
    void SetUp() override {
        PlatformFixture::setUp();
    }

    void TearDown() override {
        PlatformFixture::tearDown();
    }
};

GEN11TEST_F(Gen11PlatformCaps, WhenCheckingExtensionStringThenFp64IsNotSupported) {
    const auto &caps = pPlatform->getPlatformInfo();

    EXPECT_EQ(std::string::npos, caps.extensions.find(std::string("cl_khr_fp64")));
}
