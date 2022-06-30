/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

using clIcdGetPlatformIDsKHRTests = Test<PlatformFixture>;

namespace ULT {

TEST_F(clIcdGetPlatformIDsKHRTests, WhenPlatformIsCreatedThenDispatchLocationIsCorrect) {
    cl_platform_id platform = pPlatform;
    EXPECT_EQ((void *)platform, (void *)(&platform->dispatch));
}

TEST_F(clIcdGetPlatformIDsKHRTests, WhenGettingNumberOfPlatformsThenGreaterThanZeroIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_uint numPlatforms = 0;

    retVal = clIcdGetPlatformIDsKHR(0, nullptr, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numPlatforms, (cl_uint)0);
}

TEST_F(clIcdGetPlatformIDsKHRTests, WhenGettingExtensionFunctionAddressThenCorrectPointerIsReturned) {
    void *funPtr = clGetExtensionFunctionAddress("clIcdGetPlatformIDsKHR");
    decltype(&clIcdGetPlatformIDsKHR) expected = clIcdGetPlatformIDsKHR;
    EXPECT_NE(nullptr, funPtr);
    EXPECT_EQ(expected, reinterpret_cast<decltype(&clIcdGetPlatformIDsKHR)>(funPtr));
}

TEST_F(clIcdGetPlatformIDsKHRTests, WhenGettingPlatformIdThenCorrectIdIsReturned) {
    cl_uint numPlatforms = 0;
    cl_uint numPlatformsIcd = 0;

    auto retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clIcdGetPlatformIDsKHR(0, nullptr, &numPlatformsIcd);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(numPlatforms, numPlatformsIcd);

    std::unique_ptr<cl_platform_id, decltype(free) *> platforms(reinterpret_cast<cl_platform_id *>(malloc(sizeof(cl_platform_id) * numPlatforms)), free);
    ASSERT_NE(nullptr, platforms);

    std::unique_ptr<cl_platform_id, decltype(free) *> platformsIcd(reinterpret_cast<cl_platform_id *>(malloc(sizeof(cl_platform_id) * numPlatforms)), free);
    ASSERT_NE(nullptr, platforms);

    retVal = clGetPlatformIDs(numPlatforms, platforms.get(), nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    retVal = clIcdGetPlatformIDsKHR(numPlatformsIcd, platformsIcd.get(), nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    for (cl_uint i = 0; i < std::min(numPlatforms, numPlatformsIcd); i++) {
        EXPECT_EQ(platforms.get()[i], platformsIcd.get()[i]);
    }
}

TEST_F(clIcdGetPlatformIDsKHRTests, WhenCheckingExtensionStringThenClKhrIcdIsIncluded) {
    const ClDeviceInfo &caps = pPlatform->getClDevice(0)->getDeviceInfo();
    EXPECT_NE(std::string::npos, std::string(caps.deviceExtensions).find("cl_khr_icd"));
}
} // namespace ULT
