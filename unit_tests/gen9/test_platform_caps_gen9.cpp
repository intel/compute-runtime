/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/device/device.h"
#include "test.h"
#include "unit_tests/fixtures/platform_fixture.h"

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
    if (pPlatform->getDevice(0)->getHardwareInfo().capabilityTable.ftrSupportsFP64) {
        EXPECT_NE(std::string::npos, caps.extensions.find(std::string("cl_khr_fp64")));
    } else {
        EXPECT_EQ(std::string::npos, caps.extensions.find(std::string("cl_khr_fp64")));
    }
}

GEN9TEST_F(Gen9PlatformCaps, SKLVersion) {
    char *paramValue = new char[12];
    cl_int retVal = clGetPlatformInfo(pPlatform, CL_PLATFORM_VERSION, 12, paramValue, nullptr);
    if (pPlatform->getDevice(0)->getHardwareInfo().platform.eProductFamily == IGFX_SKYLAKE) {
        EXPECT_STREQ(paramValue, "OpenCL 2.1 ");
    }
    EXPECT_EQ(retVal, CL_SUCCESS);
    delete[] paramValue;
}

GEN9TEST_F(Gen9PlatformCaps, BXTVersion) {
    char *paramValue = new char[12];
    cl_int retVal = clGetPlatformInfo(pPlatform, CL_PLATFORM_VERSION, 12, paramValue, nullptr);
    if (pPlatform->getDevice(0)->getHardwareInfo().platform.eProductFamily == IGFX_BROXTON) {
        EXPECT_STREQ(paramValue, "OpenCL 1.2 ");
    }
    EXPECT_EQ(retVal, CL_SUCCESS);
    delete[] paramValue;
}
