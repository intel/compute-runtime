/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/source/global_teardown/global_platform_teardown.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

struct ClRetainReleaseDeviceTests : Test<PlatformFixture> {
    void SetUp() override {
        debugManager.flags.CreateMultipleRootDevices.set(maxRootDeviceCount);
        Test<PlatformFixture>::SetUp();
    }
    DebugManagerStateRestore restorer;
    const uint32_t rootDeviceIndex = 1u;
};

namespace ULT {
TEST_F(ClRetainReleaseDeviceTests, GivenRootDeviceWhenRetainingThenReferenceCountIsOne) {
    cl_uint numEntries = maxRootDeviceCount;
    cl_device_id devices[maxRootDeviceCount];

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, numEntries, devices,
                                 nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clRetainDevice(devices[rootDeviceIndex]);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clRetainDevice(devices[rootDeviceIndex]);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint theRef;
    retVal = clGetDeviceInfo(devices[rootDeviceIndex], CL_DEVICE_REFERENCE_COUNT,
                             sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);
}

TEST_F(ClRetainReleaseDeviceTests, GivenRootDeviceWhenReleasingThenReferenceCountIsOne) {
    constexpr cl_uint numEntries = maxRootDeviceCount;
    cl_device_id devices[maxRootDeviceCount];

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, numEntries, devices,
                                 nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseDevice(devices[rootDeviceIndex]);
    EXPECT_EQ(CL_SUCCESS, retVal);

    retVal = clReleaseDevice(devices[rootDeviceIndex]);
    EXPECT_EQ(CL_SUCCESS, retVal);

    cl_uint theRef;
    retVal = clGetDeviceInfo(devices[rootDeviceIndex], CL_DEVICE_REFERENCE_COUNT,
                             sizeof(cl_uint), &theRef, NULL);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, theRef);
}

TEST_F(ClRetainReleaseDeviceTests, GivenInvaliddeviceWhenCallingReleaseThenErrorCodeReturn) {
    auto retVal = clReleaseDevice(nullptr);
    EXPECT_NE(CL_SUCCESS, retVal);
}

TEST_F(ClRetainReleaseDeviceTests, GivenInvalidDeviceWhenTerdownWasCalledThenSuccessReturned) {
    wasPlatformTeardownCalled = true;
    auto retVal = clReleaseDevice(nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
    wasPlatformTeardownCalled = false;
}
} // namespace ULT
