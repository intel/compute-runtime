/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/device/device.h"
#include "runtime/helpers/options.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "test.h"

using namespace OCLRT;

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
    if (pPlatform->getDevice(0)->getHardwareInfo().pPlatform->eProductFamily == IGFX_SKYLAKE) {
        EXPECT_STREQ(paramValue, "OpenCL 2.1 ");
    }
    EXPECT_EQ(retVal, CL_SUCCESS);
    delete[] paramValue;
}

GEN9TEST_F(Gen9PlatformCaps, BXTVersion) {
    char *paramValue = new char[12];
    cl_int retVal = clGetPlatformInfo(pPlatform, CL_PLATFORM_VERSION, 12, paramValue, nullptr);
    if (pPlatform->getDevice(0)->getHardwareInfo().pPlatform->eProductFamily == IGFX_BROXTON) {
        EXPECT_STREQ(paramValue, "OpenCL 1.2 ");
    }
    EXPECT_EQ(retVal, CL_SUCCESS);
    delete[] paramValue;
}
