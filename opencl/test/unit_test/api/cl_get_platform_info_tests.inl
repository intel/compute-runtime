/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "opencl/source/platform/platform.h"
#include "test.h"

#include "CL/cl_ext.h"
#include "cl_api_tests.h"

using namespace NEO;

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

TEST_F(clGetPlatformInfoTests, GivenClPlatformProfileWhenGettingPlatformInfoStringThenFullProfileIsReturned) {
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

TEST_P(clGetPlatformInfoParameterizedTests, GivenClPlatformVersionWhenGettingPlatformInfoStringThenCorrectOpenClVersionIsReturned) {
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

TEST_F(clGetPlatformInfoTests, GivenClPlatformNameWhenGettingPlatformInfoStringThenCorrectStringIsReturned) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_NAME);
    EXPECT_STREQ(paramValue, "Intel(R) OpenCL HD Graphics");
}

TEST_F(clGetPlatformInfoTests, GivenClPlatformVendorWhenGettingPlatformInfoStringThenCorrectStringIsReturned) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_VENDOR);
    EXPECT_STREQ(paramValue, "Intel(R) Corporation");
}

TEST_F(clGetPlatformInfoTests, GivenClPlatformExtensionsWhenGettingPlatformInfoStringThenExtensionStringIsReturned) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_EXTENSIONS);

    EXPECT_NE(nullptr, strstr(paramValue, "cl_khr_icd "));
    EXPECT_NE(nullptr, strstr(paramValue, "cl_khr_fp16 "));
}

TEST_F(clGetPlatformInfoTests, GivenClPlatformIcdSuffixKhrWhenGettingPlatformInfoStringThenIntelIsReturned) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_ICD_SUFFIX_KHR);
    EXPECT_STREQ(paramValue, "INTEL");
}

TEST_F(clGetPlatformInfoTests, GivenClPlatformHostTimerResolutionWhenGettingPlatformInfoStringThenCorrectResolutionIsReturned) {
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

TEST_F(clGetPlatformInfoTests, GivenNullPlatformWhenGettingPlatformInfoStringThenClInvalidPlatformErrorIsReturned) {
    char extensions[512];
    retVal = clGetPlatformInfo(
        nullptr, // invalid platform
        CL_PLATFORM_EXTENSIONS,
        sizeof(extensions),
        extensions,
        &retSize);

    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(clGetPlatformInfoTests, GivenInvalidParamNameWhenGettingPlatformInfoStringThenClInvalidValueErrorIsReturned) {
    char extensions[512];
    retVal = clGetPlatformInfo(
        pPlatform,
        0, // invalid platform info enum
        sizeof(extensions),
        extensions,
        &retSize);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetPlatformInfoTests, GivenInvalidParamSizeWhenGettingPlatformInfoStringThenClInvalidValueErrorIsReturned) {
    char extensions[512];
    retVal = clGetPlatformInfo(
        pPlatform,
        CL_PLATFORM_EXTENSIONS,
        0, // invalid size
        extensions,
        &retSize);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetPlatformInfoTests, GivenDeviceWhenGettingIcdDispatchTableThenDeviceAndPlatformTablesMatch) {
    EXPECT_NE(pPlatform->dispatch.icdDispatch, nullptr);
    for (size_t deviceOrdinal = 0; deviceOrdinal < pPlatform->getNumDevices(); ++deviceOrdinal) {
        auto device = pPlatform->getClDevice(deviceOrdinal);
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

TEST_P(GetPlatformInfoTests, GivenValidParamWhenGettingPlatformInfoStringThenNonEmptyStringIsReturned) {
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
