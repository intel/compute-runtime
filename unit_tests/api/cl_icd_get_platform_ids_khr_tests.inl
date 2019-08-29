/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#if defined(_WIN32)
#include "core/os_interface/windows/windows_wrapper.h"
#endif

#include "runtime/device/device.h"
#include "runtime/platform/platform.h"
#include "unit_tests/api/cl_api_tests.h"

#include <algorithm>

using namespace NEO;

typedef api_tests clIcdGetPlatformIDsKHRTests;

namespace ULT {

TEST_F(clIcdGetPlatformIDsKHRTests, checkDispatchLocation) {
    cl_platform_id platform = pPlatform;
    EXPECT_EQ((void *)platform, (void *)(&platform->dispatch));
}

TEST_F(clIcdGetPlatformIDsKHRTests, getCount) {
    cl_int retVal = CL_SUCCESS;
    cl_uint numPlatforms = 0;

    retVal = clIcdGetPlatformIDsKHR(0, nullptr, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numPlatforms, (cl_uint)0);
}

TEST_F(clIcdGetPlatformIDsKHRTests, checkExtensionFunctionAvailability) {
    void *funPtr = clGetExtensionFunctionAddress("clIcdGetPlatformIDsKHR");
    decltype(&clIcdGetPlatformIDsKHR) expected = clIcdGetPlatformIDsKHR;
    EXPECT_NE(nullptr, funPtr);
    EXPECT_EQ(expected, reinterpret_cast<decltype(&clIcdGetPlatformIDsKHR)>(funPtr));
}

TEST_F(clIcdGetPlatformIDsKHRTests, checkDeviceId) {
    cl_uint numPlatforms = 0;
    cl_uint numPlatformsIcd = 0;

    retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
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

TEST_F(clIcdGetPlatformIDsKHRTests, checkExtensionString) {
    const DeviceInfo &caps = pPlatform->getDevice(0)->getDeviceInfo();
    EXPECT_NE(std::string::npos, std::string(caps.deviceExtensions).find("cl_khr_icd"));
}
} // namespace ULT
