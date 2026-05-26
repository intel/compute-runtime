/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/platform/platform.h"
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

} // namespace ult
} // namespace LEO
} // namespace NEO
