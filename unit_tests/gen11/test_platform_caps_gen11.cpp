/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/options.h"
#include "test.h"
#include "unit_tests/fixtures/platform_fixture.h"

using namespace NEO;

struct Gen11PlatformCaps : public PlatformFixture, public ::testing::Test {
    void SetUp() override {
        PlatformFixture::SetUp();
    }

    void TearDown() override {
        PlatformFixture::TearDown();
    }
};

ICLLPTEST_F(Gen11PlatformCaps, lpSkusDontSupportFP64) {
    const auto &caps = pPlatform->getPlatformInfo();

    EXPECT_EQ(std::string::npos, caps.extensions.find(std::string("cl_khr_fp64")));
}
