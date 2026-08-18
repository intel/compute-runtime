/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl_ext.h"

#include <string>
#include <vector>

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

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoNameThenReturnsNullTerminatedString) {
    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_NAME, 0, nullptr, &retSize));
    ASSERT_GT(retSize, 0u);

    std::string name(retSize, 'x');
    EXPECT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_NAME, retSize, name.data(), nullptr));
    EXPECT_EQ('\0', name[retSize - 1]);
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoExtensionsThenReturnsNullTerminatedString) {
    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_EXTENSIONS, 0, nullptr, &retSize));
    ASSERT_GT(retSize, 0u);

    std::string extensions(retSize, 'x');
    EXPECT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_EXTENSIONS, retSize, extensions.data(), nullptr));
    EXPECT_EQ('\0', extensions[retSize - 1]);
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoHostTimerResolutionThenReturnsUint64) {
    uint64_t resolution = 0;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_HOST_TIMER_RESOLUTION, sizeof(resolution), &resolution, &retSize));
    EXPECT_EQ(sizeof(uint64_t), retSize);
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoExtensionsWithVersionThenSizeIsMultipleOfNameVersion) {
    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_EXTENSIONS_WITH_VERSION, 0, nullptr, &retSize));
    ASSERT_GT(retSize, 0u);
    EXPECT_EQ(0u, retSize % sizeof(cl_name_version));

    std::vector<cl_name_version> extensions(retSize / sizeof(cl_name_version));
    EXPECT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_EXTENSIONS_WITH_VERSION, retSize, extensions.data(), nullptr));
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoExtensionsWithVersionTwiceThenSizeIsStable) {
    size_t firstSize = 0;
    size_t secondSize = 0;
    ASSERT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_EXTENSIONS_WITH_VERSION, 0, nullptr, &firstSize));
    ASSERT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_EXTENSIONS_WITH_VERSION, 0, nullptr, &secondSize));
    EXPECT_EQ(firstSize, secondSize);
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoExternalMemoryImportHandleTypesThenQuerySucceeds) {
    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR, 0, nullptr, &retSize));
    ASSERT_GT(retSize, 0u);

    std::vector<uint8_t> value(retSize);
    EXPECT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR, retSize, value.data(), nullptr));
}

TEST_F(PlatformGetInfoTests, givenPlatformWhenGetInfoL0DriverHandleThenReturnsOwningDriver) {
    ze_driver_handle_t queried = nullptr;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, platform->getInfo(CL_L0_DRIVER_HANDLE, sizeof(queried), &queried, &retSize));
    EXPECT_EQ(sizeof(ze_driver_handle_t), retSize);
    EXPECT_EQ(driverHandle->toHandle(), queried);
}

TEST_F(PlatformGetInfoTests, givenTooSmallBufferWhenGetInfoThenReturnsCLInvalidValue) {
    const cl_platform_info params[] = {CL_PLATFORM_VERSION, CL_PLATFORM_PROFILE, CL_PLATFORM_NAME,
                                       CL_PLATFORM_VENDOR, CL_PLATFORM_NUMERIC_VERSION,
                                       CL_PLATFORM_HOST_TIMER_RESOLUTION, CL_L0_DRIVER_HANDLE};

    std::vector<uint8_t> storage(4096);
    for (auto paramName : params) {
        size_t retSize = 0;
        ASSERT_EQ(CL_SUCCESS, platform->getInfo(paramName, 0, nullptr, &retSize)) << "param 0x" << std::hex << paramName;
        ASSERT_GT(retSize, 0u);
        EXPECT_EQ(CL_INVALID_VALUE, platform->getInfo(paramName, retSize - 1, storage.data(), nullptr))
            << "param 0x" << std::hex << paramName;
    }
}

TEST_F(PlatformGetInfoTests, givenInvalidParamWhenGetInfoThenReturnSizeIsNotOverwritten) {
    size_t retSize = 4242u;
    EXPECT_EQ(CL_INVALID_VALUE, platform->getInfo(0xDEAD0000u, 0, nullptr, &retSize));
    EXPECT_EQ(4242u, retSize);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
