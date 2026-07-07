/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/hw_test.h"
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

HWTEST2_F(GetPlatformIDsWithDeviceTests, givenDefaultFlagAndCriProductWhenGetPlatformIDsThenReturnsPlatforms, IsCRI) {
    cl_uint numPlatforms = 0;
    auto retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numPlatforms);
}

HWTEST2_F(GetPlatformIDsWithDeviceTests, givenDefaultFlagAndNonCriProductWhenGetPlatformIDsThenReturnsNoPlatforms, IsNotCRI) {
    cl_uint numPlatforms = 1u;
    auto retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, numPlatforms);
}

HWTEST2_F(GetPlatformIDsWithDeviceTests, givenFlag1AndNonCriProductWhenGetPlatformIDsThenReturnsPlatforms, IsNotCRI) {
    debugManager.flags.EnableLEO.set(1);
    cl_uint numPlatforms = 0;
    auto retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numPlatforms);
}

HWTEST2_F(GetPlatformIDsWithDeviceTests, givenFlag0AndCriProductWhenGetPlatformIDsThenReturnsNoPlatforms, IsCRI) {
    debugManager.flags.EnableLEO.set(0);
    cl_uint numPlatforms = 1u;
    auto retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, numPlatforms);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
