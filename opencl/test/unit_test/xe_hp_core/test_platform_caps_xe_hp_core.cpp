/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

struct XE_HP_COREPlatformCaps : public PlatformFixture, public ::testing::Test {
    void SetUp() override {
        PlatformFixture::setUp();
    }

    void TearDown() override {
        PlatformFixture::tearDown();
    }
};

HWTEST2_F(XE_HP_COREPlatformCaps, givenXeHPSkusThenItSupportFP64, IsXEHP) {
    const auto &caps = pPlatform->getPlatformInfo();

    EXPECT_NE(std::string::npos, caps.extensions.find(std::string("cl_khr_fp64")));
}