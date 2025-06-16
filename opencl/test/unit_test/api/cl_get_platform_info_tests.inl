/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/platform/platform_info.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

struct ClGetPlatformInfoTests : Test<PlatformFixture> {
    void SetUp() override {
        Test<PlatformFixture>::SetUp();
    }

    void TearDown() override {
        delete[] paramValue;
        Test<PlatformFixture>::TearDown();
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

    size_t retSize = 0;
    char *paramValue = nullptr;
};

namespace ULT {

TEST_F(ClGetPlatformInfoTests, GivenClPlatformProfileWhenGettingPlatformInfoStringThenFullProfileIsReturned) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_PROFILE);
    EXPECT_STREQ(paramValue, "FULL_PROFILE");
}

class ClGetPlatformInfoParameterizedTests : public ClGetPlatformInfoTests,
                                            public ::testing::WithParamInterface<uint32_t> {
    void SetUp() override {
        debugManager.flags.ForceOCLVersion.set(GetParam());
        ClGetPlatformInfoTests::SetUp();
    }

    void TearDown() override {
        ClGetPlatformInfoTests::TearDown();
        debugManager.flags.ForceOCLVersion.set(0);
    }
};

TEST_P(ClGetPlatformInfoParameterizedTests, GivenClPlatformVersionWhenGettingPlatformInfoStringThenCorrectOpenClVersionIsReturned) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_VERSION);

    cl_version platformNumericVersion = 0;
    auto retVal = clGetPlatformInfo(pPlatform, CL_PLATFORM_NUMERIC_VERSION,
                                    sizeof(platformNumericVersion), &platformNumericVersion, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_version), retSize);

    std::string expectedPlatformVersion;
    cl_version expectedNumericPlatformVersion;
    switch (GetParam()) {
    case 30:
        expectedPlatformVersion = "OpenCL 3.0 ";
        expectedNumericPlatformVersion = CL_MAKE_VERSION(3, 0, 0);
        break;
    case 21:
        expectedPlatformVersion = "OpenCL 2.1 ";
        expectedNumericPlatformVersion = CL_MAKE_VERSION(2, 1, 0);
        break;
    case 12:
    default:
        expectedPlatformVersion = "OpenCL 1.2 ";
        expectedNumericPlatformVersion = CL_MAKE_VERSION(1, 2, 0);
        break;
    }

    EXPECT_STREQ(expectedPlatformVersion.c_str(), paramValue);
    EXPECT_EQ(expectedNumericPlatformVersion, platformNumericVersion);
}

INSTANTIATE_TEST_SUITE_P(OCLVersions,
                         ClGetPlatformInfoParameterizedTests,
                         ::testing::Values(12, 21, 30));

TEST_F(ClGetPlatformInfoTests, GivenClPlatformNameWhenGettingPlatformInfoStringThenCorrectStringIsReturned) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_NAME);
    EXPECT_STREQ(paramValue, "Intel(R) OpenCL Graphics");
}

class ClGetPlatformInfoOverridePlatformNameTests : public ClGetPlatformInfoTests {
  public:
    void SetUp() override {
        NEO::debugManager.flags.OverridePlatformName.set(testPlatformName);
        ClGetPlatformInfoTests::SetUp();
    }

    void TearDown() override {
        ClGetPlatformInfoTests::TearDown();
    }

    DebugManagerStateRestore restorer;
    const std::string testPlatformName = "test platform name";
};

TEST_F(ClGetPlatformInfoOverridePlatformNameTests, givenDebugVariableOverridePlatformNameSpecifiedWhenGettingPlatformInfoStringThenPlatformNameIsTakenFromDebugVariable) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_NAME);
    EXPECT_STREQ(paramValue, testPlatformName.c_str());
}

TEST_F(ClGetPlatformInfoTests, GivenClPlatformVendorWhenGettingPlatformInfoStringThenCorrectStringIsReturned) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_VENDOR);
    EXPECT_STREQ(paramValue, "Intel(R) Corporation");
}

TEST_F(ClGetPlatformInfoTests, GivenClPlatformExtensionsWhenGettingPlatformInfoStringThenExtensionStringIsReturned) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_EXTENSIONS);

    EXPECT_NE(nullptr, strstr(paramValue, "cl_khr_icd "));
    EXPECT_NE(nullptr, strstr(paramValue, "cl_khr_fp16 "));
}

TEST_F(ClGetPlatformInfoTests, GivenClPlatformIcdSuffixKhrWhenGettingPlatformInfoStringThenIntelIsReturned) {
    paramValue = getPlatformInfoString(pPlatform, CL_PLATFORM_ICD_SUFFIX_KHR);
    EXPECT_STREQ(paramValue, "INTEL");
}

TEST_F(ClGetPlatformInfoTests, GivenClPlatformHostTimerResolutionWhenGettingPlatformInfoStringThenCorrectResolutionIsReturned) {
    auto retVal = clGetPlatformInfo(pPlatform, CL_PLATFORM_HOST_TIMER_RESOLUTION, 0, nullptr, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(retSize, 0u);

    cl_ulong value = 0;
    retVal = clGetPlatformInfo(pPlatform, CL_PLATFORM_HOST_TIMER_RESOLUTION, retSize, &value, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto device = pPlatform->getClDevice(0);
    cl_ulong resolution = static_cast<cl_ulong>(device->getPlatformHostTimerResolution());
    EXPECT_EQ(resolution, value);
}

TEST_F(ClGetPlatformInfoTests, GivenNullPlatformWhenGettingPlatformInfoStringThenClInvalidPlatformErrorIsReturned) {
    char extensions[512];
    auto retVal = clGetPlatformInfo(
        nullptr, // invalid platform
        CL_PLATFORM_EXTENSIONS,
        sizeof(extensions),
        extensions,
        &retSize);

    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(ClGetPlatformInfoTests, GivenInvalidParamNameWhenGettingPlatformInfoStringThenClInvalidValueErrorIsReturned) {
    char extensions[512];
    auto retVal = clGetPlatformInfo(
        pPlatform,
        0, // invalid platform info enum
        sizeof(extensions),
        extensions,
        &retSize);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClGetPlatformInfoTests, GivenInvalidParametersWhenGettingPlatformInfoThenValueSizeRetIsNotUpdated) {
    char extensions[512];
    retSize = 0x1234;
    auto retVal = clGetPlatformInfo(
        pPlatform,
        0, // invalid platform info enum
        sizeof(extensions),
        extensions,
        &retSize);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0x1234u, retSize);
}

TEST_F(ClGetPlatformInfoTests, GivenInvalidParamSizeWhenGettingPlatformInfoStringThenClInvalidValueErrorIsReturned) {
    char extensions[512];
    auto retVal = clGetPlatformInfo(
        pPlatform,
        CL_PLATFORM_EXTENSIONS,
        0, // invalid size
        extensions,
        &retSize);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(ClGetPlatformInfoTests, GivenDeviceWhenGettingIcdDispatchTableThenDeviceAndPlatformTablesMatch) {
    EXPECT_NE(pPlatform->dispatch.icdDispatch, nullptr);
    for (size_t deviceOrdinal = 0; deviceOrdinal < pPlatform->getNumDevices(); ++deviceOrdinal) {
        auto device = pPlatform->getClDevice(deviceOrdinal);
        ASSERT_NE(nullptr, device);
        EXPECT_EQ(pPlatform->dispatch.icdDispatch, device->dispatch.icdDispatch);
    }
}

TEST_F(ClGetPlatformInfoTests, WhenCheckingPlatformExtensionsWithVersionThenTheyMatchPlatformExtensions) {

    auto retVal = clGetPlatformInfo(pPlatform, CL_PLATFORM_EXTENSIONS_WITH_VERSION, 0, nullptr, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);

    size_t extensionsCount = retSize / sizeof(cl_name_version);
    auto platformExtensionsWithVersion = std::make_unique<cl_name_version[]>(extensionsCount);
    retVal = clGetPlatformInfo(pPlatform, CL_PLATFORM_EXTENSIONS_WITH_VERSION,
                               retSize, platformExtensionsWithVersion.get(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);

    std::string allExtensions;
    for (size_t i = 0; i < extensionsCount; i++) {
        if (strcmp(platformExtensionsWithVersion[i].name, "cl_khr_integer_dot_product") == 0) {
            EXPECT_EQ(CL_MAKE_VERSION(2u, 0, 0), platformExtensionsWithVersion[i].version);
        } else if (strcmp(platformExtensionsWithVersion[i].name, "cl_intel_unified_shared_memory") == 0) {
            EXPECT_EQ(CL_MAKE_VERSION(1u, 1u, 0), platformExtensionsWithVersion[i].version);
        } else if (strcmp(platformExtensionsWithVersion[i].name, "cl_khr_external_memory") == 0) {
            EXPECT_EQ(CL_MAKE_VERSION(0, 9u, 1u), platformExtensionsWithVersion[i].version);
        } else {
            EXPECT_EQ(CL_MAKE_VERSION(1u, 0, 0), platformExtensionsWithVersion[i].version);
        }
        allExtensions += platformExtensionsWithVersion[i].name;
        allExtensions += " ";
    }

    EXPECT_STREQ(pPlatform->getPlatformInfo().extensions.c_str(), allExtensions.c_str());
}

class GetPlatformInfoTests : public PlatformFixture,
                             public testing::TestWithParam<uint32_t /*cl_platform_info*/> {
    using PlatformFixture::setUp;

  public:
    GetPlatformInfoTests() {}

  protected:
    void SetUp() override {
        platformInfo = GetParam();
        PlatformFixture::setUp();
    }

    void TearDown() override {
        PlatformFixture::tearDown();
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

const cl_platform_info platformInfoTestValues[] =
    {
        CL_PLATFORM_PROFILE,
        CL_PLATFORM_VERSION,
        CL_PLATFORM_NAME,
        CL_PLATFORM_VENDOR,
        CL_PLATFORM_EXTENSIONS,
        CL_PLATFORM_ICD_SUFFIX_KHR,
};

INSTANTIATE_TEST_SUITE_P(api,
                         GetPlatformInfoTests,
                         ::testing::ValuesIn(platformInfoTestValues));
} // namespace ULT
