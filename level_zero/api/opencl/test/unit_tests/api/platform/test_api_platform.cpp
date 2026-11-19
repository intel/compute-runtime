/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"

#include "CL/cl.h"

namespace NEO {
namespace LEO {
namespace ult {

TEST(GetPlatformIDsTests, givenZeroNumEntriesAndNonNullPlatformsThenReturnsCLInvalidValue) {
    cl_platform_id platform = nullptr;
    auto retVal = clGetPlatformIDs(0, &platform, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(GetPlatformIDsTests, givenNonZeroNumEntriesAndBothOutputsNullThenReturnsCLInvalidValue) {
    auto retVal = clGetPlatformIDs(1, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(GetPlatformIDsTests, givenBothOutputsNullAndZeroNumEntriesThenReturnsSuccess) {
    VariableBackup<decltype(platformsImpl)> platformsImplBackup{&platformsImpl, nullptr};
    auto retVal = clGetPlatformIDs(0, nullptr, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(GetPlatformInfoTests, givenNullPlatformWhenGetPlatformInfoThenReturnsCLInvalidPlatform) {
    auto retVal = clGetPlatformInfo(nullptr, CL_PLATFORM_VERSION, 0, nullptr, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

using UnloadPlatformCompilerTests = Test<OclFixture>;

TEST_F(UnloadPlatformCompilerTests, givenInvalidPlatformThenReturnsCLInvalidPlatform) {
    auto retVal = clUnloadPlatformCompiler(nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(UnloadPlatformCompilerTests, givenValidPlatformThenReturnsSuccess) {
    auto retVal = clUnloadPlatformCompiler(platform);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(UnloadCompilerTests, givenNoArgsThenReturnsSuccess) {
    auto retVal = clUnloadCompiler();
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST(GetExtensionFunctionAddressTests, givenKnownExtensionNameThenReturnsNonNull) {
    auto ptr = clGetExtensionFunctionAddress("clIcdGetPlatformIDsKHR");
    EXPECT_NE(nullptr, ptr);
}

TEST(GetExtensionFunctionAddressTests, givenUnknownExtensionNameThenReturnsNull) {
    auto ptr = clGetExtensionFunctionAddress("clNonExistentExtension");
    EXPECT_EQ(nullptr, ptr);
}

struct GetPlatformIDsWithDeviceTests : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        platforms.push_back(std::unique_ptr<Platform>(platform));
        platformsImplBackup = std::make_unique<VariableBackup<decltype(platformsImpl)>>(&platformsImpl, &platforms);
    }
    void TearDown() override {
        platformsImplBackup.reset();
        platforms.back().release();
        platforms.clear();
        Test<OclFixture>::TearDown();
    }
    std::vector<std::unique_ptr<Platform>> platforms;
    std::unique_ptr<VariableBackup<decltype(platformsImpl)>> platformsImplBackup;
    DebugManagerStateRestore restorer;
};

TEST_F(GetPlatformIDsWithDeviceTests, givenDefaultFlagWhenGetPlatformIDsThenReturnsPlatforms) {
    cl_uint numPlatforms = 0;
    auto retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numPlatforms);
}

TEST_F(GetPlatformIDsWithDeviceTests, givenFlag0WhenGetPlatformIDsThenFlagIsIgnoredAndReturnsPlatforms) {
    debugManager.flags.EnableLEO.set(0);
    cl_uint numPlatforms = 0;
    auto retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numPlatforms);
}

struct LeoExposedDriverTests : public Test<OclFixture> {
    DebugManagerStateRestore restorer;
};

TEST_F(LeoExposedDriverTests, givenProductSupportingLeoThenDriverIsExposed) {
    neoDevice->getRootDeviceEnvironmentRef().getMutableHardwareInfo()->caps.leoSupported = true;
    EXPECT_TRUE(isLeoExposedDriver(driverHandle->toHandle()));
}

TEST_F(LeoExposedDriverTests, givenProductNotSupportingLeoThenDriverIsStripped) {
    neoDevice->getRootDeviceEnvironmentRef().getMutableHardwareInfo()->caps.leoSupported = false;
    EXPECT_FALSE(isLeoExposedDriver(driverHandle->toHandle()));
}

TEST_F(LeoExposedDriverTests, givenEnableLeoForcedOnThenDriverIsExposedEvenWhenProductDoesNotSupportLeo) {
    debugManager.flags.EnableLEO.set(1);

    neoDevice->getRootDeviceEnvironmentRef().getMutableHardwareInfo()->caps.leoSupported = false;
    EXPECT_TRUE(isLeoExposedDriver(driverHandle->toHandle()));
}

TEST_F(LeoExposedDriverTests, givenNullDriverHandleThenDriverIsStripped) {
    EXPECT_FALSE(isLeoExposedDriver(nullptr));
}

TEST_F(LeoExposedDriverTests, whenPlatformIsBuiltThenItAdvertisesClKhrIcd) {
    size_t extensionsSize = 0;
    EXPECT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_EXTENSIONS, 0, nullptr, &extensionsSize));
    std::string extensions(extensionsSize, '\0');
    EXPECT_EQ(CL_SUCCESS, platform->getInfo(CL_PLATFORM_EXTENSIONS, extensionsSize, extensions.data(), nullptr));
    EXPECT_NE(std::string::npos, extensions.find("cl_khr_icd"));
}

struct GetPlatformIDsDriverInitTests : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        driverHandles.push_back(driverHandle->toHandle());
    }

    void TearDown() override {
        platforms.clear();
        Test<OclFixture>::TearDown();
    }

    L0::ult::Mock<L0::ult::Driver> driver;
    std::vector<_ze_driver_handle_t *> driverHandles;
    std::vector<std::unique_ptr<Platform>> platforms;
    VariableBackup<decltype(L0::globalDriverHandles)> globalDriverHandlesBackup{&L0::globalDriverHandles, &driverHandles};
    VariableBackup<bool> levelZeroDriverInitializedBackup{&L0::levelZeroDriverInitialized, false};
    VariableBackup<bool> levelZeroDriverInitializedByLeoBackup{&L0::levelZeroDriverInitializedByLeo, false};
    VariableBackup<decltype(platformsImpl)> platformsBackup{&platformsImpl, &platforms};
    VariableBackup<bool> platformsInitAttemptedBackup{&platformsInitAttempted, false};
};

TEST_F(GetPlatformIDsDriverInitTests, givenNotInitializedDriverWhenGetPlatformIDsInitializesDriverThenDriverIsMarkedAsInitializedByLeo) {
    cl_uint numPlatforms = 0;
    EXPECT_EQ(CL_SUCCESS, clGetPlatformIDs(0, nullptr, &numPlatforms));
    EXPECT_TRUE(L0::levelZeroDriverInitialized);
    EXPECT_TRUE(L0::levelZeroDriverInitializedByLeo);
}

TEST_F(GetPlatformIDsDriverInitTests, givenDriverInitFailureWhenGetPlatformIDsThenDriverIsNotMarkedAsInitializedByLeo) {
    driver.failInitDriver = true;
    cl_uint numPlatforms = 0;
    EXPECT_NE(CL_SUCCESS, clGetPlatformIDs(0, nullptr, &numPlatforms));
    EXPECT_FALSE(L0::levelZeroDriverInitializedByLeo);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
