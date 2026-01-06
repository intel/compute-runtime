/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/platform/platform_info.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"

#include "hw_cmds_xe3p_core.h"
#include "per_product_test_definitions.h"

using namespace NEO;

struct Xe3pCorePlatformCaps : public PlatformFixture, public ::testing::Test {
    void SetUp() override {
        PlatformFixture::setUp();
    }

    void TearDown() override {
        PlatformFixture::tearDown();
    }
};

XE3P_CORETEST_F(Xe3pCorePlatformCaps, givenXe3pCoreSkusThenItSupportFP64) {
    const auto &caps = pPlatform->getPlatformInfo();

    EXPECT_NE(std::string::npos, caps.extensions.find(std::string("cl_khr_fp64")));
}
