/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

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
    RAIIProductHelperFactory<MockProductHelper> raiiProductHelper{neoDevice->getRootDeviceEnvironmentRef()};
    raiiProductHelper.mockProductHelper->isLEOSupportedResult = true;
    EXPECT_TRUE(isLeoExposedDriver(driverHandle->toHandle()));
}

TEST_F(LeoExposedDriverTests, givenProductNotSupportingLeoThenDriverIsStripped) {
    RAIIProductHelperFactory<MockProductHelper> raiiProductHelper{neoDevice->getRootDeviceEnvironmentRef()};
    raiiProductHelper.mockProductHelper->isLEOSupportedResult = false;
    EXPECT_FALSE(isLeoExposedDriver(driverHandle->toHandle()));
}

TEST_F(LeoExposedDriverTests, givenEnableLeoForcedOnThenDriverIsExposedEvenWhenProductDoesNotSupportLeo) {
    debugManager.flags.EnableLEO.set(1);

    RAIIProductHelperFactory<MockProductHelper> raiiProductHelper{neoDevice->getRootDeviceEnvironmentRef()};
    raiiProductHelper.mockProductHelper->isLEOSupportedResult = false;
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

} // namespace ult
} // namespace LEO
} // namespace NEO
