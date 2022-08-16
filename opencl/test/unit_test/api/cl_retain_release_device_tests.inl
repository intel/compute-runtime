/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

struct clRetainReleaseDeviceTests : Test<PlatformFixture> {
    void SetUp() override {
        DebugManager.flags.CreateMultipleRootDevices.set(maxRootDeviceCount);
        Test<PlatformFixture>::SetUp();
    }
    DebugManagerStateRestore restorer;
    const uint32_t rootDeviceIndex = 1u;
};

namespace ULT {
TEST_F(clRetainReleaseDeviceTests, GivenRootDeviceWhenRetainingThenReferenceCountIsOne) {
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

TEST_F(clRetainReleaseDeviceTests, GivenRootDeviceWhenReleasingThenReferenceCountIsOne) {
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
} // namespace ULT
