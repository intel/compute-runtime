/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

namespace ULT {

TEST(GetDeviceInfo, givenClDeviceWhenGettingDeviceInfoThenBadAdapterLuidIsReturned) {
    std::array<uint8_t, CL_LUID_SIZE_KHR> deviceLuidKHR, expectLuid = {0, 1, 2, 3, 4, 5, 6, 7};
    deviceLuidKHR = expectLuid;
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_LUID_KHR, CL_LUID_SIZE_KHR, deviceLuidKHR.data(), 0);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(deviceLuidKHR, expectLuid);
}

TEST(GetDeviceInfo, givenClDeviceWhenGettingDeviceInfoThenLuidIsInvalid) {
    cl_bool isValid = true;
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_LUID_VALID_KHR, sizeof(cl_bool), &isValid, 0);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_FALSE(isValid);
}

TEST(GetDeviceInfo, givenClDeviceWhenGettingDeviceInfoThenNodeMaskIsUnchanged) {
    cl_uint nodeMask = 0x1234u;
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_NODE_MASK_KHR, sizeof(cl_uint), &nodeMask, 0);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(nodeMask, 0x1234u);
}
} // namespace ULT
