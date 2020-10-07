/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "test.h"

using namespace NEO;

struct Gen9PlatformCaps : public PlatformFixture, public ::testing::Test {
    void SetUp() override {
        PlatformFixture::SetUp();
    }

    void TearDown() override {
        PlatformFixture::TearDown();
    }
};

GEN9TEST_F(Gen9PlatformCaps, allSkusSupportFP64) {
    const auto &caps = pPlatform->getPlatformInfo();
    if (pPlatform->getClDevice(0)->getHardwareInfo().capabilityTable.ftrSupportsFP64) {
        EXPECT_NE(std::string::npos, caps.extensions.find(std::string("cl_khr_fp64")));
    } else {
        EXPECT_EQ(std::string::npos, caps.extensions.find(std::string("cl_khr_fp64")));
    }
}
