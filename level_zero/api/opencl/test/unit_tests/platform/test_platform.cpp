/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include <string>

namespace NEO {
namespace LEO {
namespace ult {

using PlatformGetInfoTests = Test<OclFixture>;

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoVersionThenReturnsOpenCL30String) {
    size_t retSize = 0;
    auto retVal = platform->getInfo(CL_PLATFORM_VERSION, 0, nullptr, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(retSize, 0u);

    std::string version(retSize, '\0');
    retVal = platform->getInfo(CL_PLATFORM_VERSION, retSize, version.data(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(std::string::npos, version.find("OpenCL 3.0"));
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoProfileThenReturnsFullProfile) {
    size_t retSize = 0;
    auto retVal = platform->getInfo(CL_PLATFORM_PROFILE, 0, nullptr, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(retSize, 0u);

    std::string profile(retSize, '\0');
    retVal = platform->getInfo(CL_PLATFORM_PROFILE, retSize, profile.data(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(std::string::npos, profile.find("FULL_PROFILE"));
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoVendorThenReturnsIntelString) {
    size_t retSize = 0;
    auto retVal = platform->getInfo(CL_PLATFORM_VENDOR, 0, nullptr, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(retSize, 0u);

    std::string vendor(retSize, '\0');
    retVal = platform->getInfo(CL_PLATFORM_VENDOR, retSize, vendor.data(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(std::string::npos, vendor.find("Intel"));
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoIcdSuffixThenReturnsIntel) {
    size_t retSize = 0;
    auto retVal = platform->getInfo(CL_PLATFORM_ICD_SUFFIX_KHR, 0, nullptr, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(retSize, 0u);

    std::string icdSuffix(retSize, '\0');
    retVal = platform->getInfo(CL_PLATFORM_ICD_SUFFIX_KHR, retSize, icdSuffix.data(), nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(std::string::npos, icdSuffix.find("INTEL"));
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoWithInvalidParamThenReturnsCLInvalidValue) {
    auto retVal = platform->getInfo(static_cast<cl_platform_info>(0xDEAD), 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoNumericVersionThenReturnsCL30) {
    auto numericVersion = 0;
    size_t retSize = 0;
    auto retVal = platform->getInfo(CL_PLATFORM_NUMERIC_VERSION, sizeof(numericVersion), &numericVersion, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_version), retSize);
    EXPECT_EQ(CL_MAKE_VERSION(3, 0, 0), numericVersion);
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetDevicesThenReturnsNonEmptyList) {
    EXPECT_FALSE(platform->getDevices().empty());
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoNameWithNullBufferThenReturnsSizeOnly) {
    size_t retSize = 0;
    auto retVal = platform->getInfo(CL_PLATFORM_NAME, 0, nullptr, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(retSize, 0u);
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoUnloadableThenReturnsCLTrue) {
    int unloadable = false;
    size_t retSize = 0;
    auto retVal = platform->getInfo(CL_PLATFORM_UNLOADABLE_KHR, sizeof(unloadable), &unloadable, &retSize);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(sizeof(cl_bool), retSize);
    EXPECT_EQ(CL_TRUE, unloadable);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
