/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cl_api_tests.h"
#include "runtime/device/device.h"
#include "runtime/helpers/options.h"
#include "runtime/platform/platform.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "CL/cl_ext.h"
#include "test.h"

using namespace OCLRT;

struct clGetPlatformInfoTests : public api_tests {
    void SetUp() override {
        api_tests::SetUp();
    }

    void TearDown() override {
        delete[] paramValue;
        api_tests::TearDown();
    }

    char *getPlatformInfoString(Platform *pPlatform, cl_platform_info paramName) {
        size_t retSize;

        auto retVal = clGetPlatformInfo(pPlatform, paramName, 0, nullptr, &retSize);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_GT(retSize, 0u);

        auto value = new char[retSize];
        retVal = clGetPlatformInfo(pPlatform, paramName, retSize, value, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        return value;
    }

    char *paramValue = nullptr;
};

namespace ULT {

TEST_F(clGetPlatformInfoTests, Profile) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_PROFILE);
    EXPECT_STREQ(paramValue, "FULL_PROFILE");
}

class clGetPlatformInfoParameterizedTests : public clGetPlatformInfoTests,
                                            public ::testing::WithParamInterface<uint32_t> {
    void SetUp() override {
        DebugManager.flags.ForceOCLVersion.set(GetParam());
        clGetPlatformInfoTests::SetUp();
    }

    void TearDown() override {
        clGetPlatformInfoTests::TearDown();
        DebugManager.flags.ForceOCLVersion.set(0);
    }
};

TEST_P(clGetPlatformInfoParameterizedTests, Version) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_VERSION);
    std::string deviceVer;
    switch (GetParam()) {
    case 21:
        deviceVer = "OpenCL 2.1 ";
        break;
    case 20:
        deviceVer = "OpenCL 2.0 ";
        break;
    case 12:
    default:
        deviceVer = "OpenCL 1.2 ";
        break;
    }
    EXPECT_STREQ(paramValue, deviceVer.c_str());
}

INSTANTIATE_TEST_CASE_P(OCLVersions,
                        clGetPlatformInfoParameterizedTests,
                        ::testing::Values(12, 20, 21));

TEST_F(clGetPlatformInfoTests, Name) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_NAME);
    EXPECT_STREQ(paramValue, "Intel(R) OpenCL HD Graphics");
}

TEST_F(clGetPlatformInfoTests, Vendor) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_VENDOR);
    EXPECT_STREQ(paramValue, "Intel(R) Corporation");
}

TEST_F(clGetPlatformInfoTests, Extensions) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_EXTENSIONS);

    EXPECT_NE(nullptr, strstr(paramValue, "cl_khr_icd "));
    EXPECT_NE(nullptr, strstr(paramValue, "cl_khr_fp16 "));
}

TEST_F(clGetPlatformInfoTests, Suffix) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_ICD_SUFFIX_KHR);
    EXPECT_STREQ(paramValue, "INTEL");
}

TEST_F(clGetPlatformInfoTests, TimerResolution) {
    auto retVal = clGetPlatformInfo(pPlatform, CL_PLATFORM_HOST_TIMER_RESOLUTION, 0, nullptr, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(retSize, 0u);

    cl_ulong value = 0;
    retVal = clGetPlatformInfo(pPlatform, CL_PLATFORM_HOST_TIMER_RESOLUTION, retSize, &value, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto device = pPlatform->getDevice(0);
    cl_ulong resolution = static_cast<cl_ulong>(device->getPlatformHostTimerResolution());
    EXPECT_EQ(resolution, value);
}

TEST_F(clGetPlatformInfoTests, NullPlatformReturnsError) {
    char extensions[512];
    retVal = clGetPlatformInfo(
        nullptr, // invalid platform
        CL_PLATFORM_EXTENSIONS,
        sizeof(extensions),
        extensions,
        &retSize);

    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(clGetPlatformInfoTests, InvalidParamNameReturnsError) {
    char extensions[512];
    retVal = clGetPlatformInfo(
        pPlatform,
        0, // invalid platform info enum
        sizeof(extensions),
        extensions,
        &retSize);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetPlatformInfoTests, InvalidParamSize_returnsError) {
    char extensions[512];
    retVal = clGetPlatformInfo(
        pPlatform,
        CL_PLATFORM_EXTENSIONS,
        0, // invalid size
        extensions,
        &retSize);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetPlatformInfoTests, DispatchTable) {
    EXPECT_NE(pPlatform->dispatch.icdDispatch, nullptr);
    for (size_t deviceOrdinal = 0; deviceOrdinal < pPlatform->getNumDevices(); ++deviceOrdinal) {
        auto device = pPlatform->getDevice(deviceOrdinal);
        ASSERT_NE(nullptr, device);
        EXPECT_EQ(pPlatform->dispatch.icdDispatch, device->dispatch.icdDispatch);
    }
}

class GetPlatformInfoTests : public PlatformFixture,
                             public testing::TestWithParam<uint32_t /*cl_platform_info*/> {
    using PlatformFixture::SetUp;

  public:
    GetPlatformInfoTests() {}

  protected:
    void SetUp() override {
        platformInfo = GetParam();
        PlatformFixture::SetUp();
    }

    void TearDown() override {
        PlatformFixture::TearDown();
    }

    char *getPlatformInfoString(Platform *pPlatform, cl_platform_info paramName) {
        size_t retSize;

        auto retVal = clGetPlatformInfo(pPlatform, paramName, 0, nullptr, &retSize);
        EXPECT_EQ(CL_SUCCESS, retVal);
        EXPECT_GT(retSize, 0u);

        auto value = new char[retSize];
        retVal = clGetPlatformInfo(pPlatform, paramName, retSize, value, nullptr);
        EXPECT_EQ(CL_SUCCESS, retVal);

        return value;
    }

    cl_int retVal = CL_SUCCESS;
    size_t retSize = 0;
    cl_platform_info platformInfo = 0;
    const HardwareInfo *pHwInfo = nullptr;
};

TEST_P(GetPlatformInfoTests, Param) {
    auto paramValue = getPlatformInfoString(pPlatform, platformInfo);

    EXPECT_STRNE(paramValue, "");

    delete[] paramValue;
}

const cl_platform_info PlatformInfoTestValues[] =
    {
        CL_PLATFORM_PROFILE,
        CL_PLATFORM_VERSION,
        CL_PLATFORM_NAME,
        CL_PLATFORM_VENDOR,
        CL_PLATFORM_EXTENSIONS,
        CL_PLATFORM_ICD_SUFFIX_KHR,
};

INSTANTIATE_TEST_CASE_P(api,
                        GetPlatformInfoTests,
                        ::testing::ValuesIn(PlatformInfoTestValues));
} // namespace ULT
