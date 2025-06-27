/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace ULT {

TEST(GetDeviceInfo, givenClDeviceWhenGettingDeviceInfoLuidSizeThenCorrectLuidSizeIsReturned) {
    size_t sizeReturned = std::numeric_limits<size_t>::max();
    size_t expectedSize = CL_LUID_SIZE_KHR;
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_LUID_KHR, 0, nullptr, &sizeReturned);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(sizeReturned, expectedSize);
}

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

TEST(GetDeviceInfo, givenClDeviceWhenGettingDeviceInfoNodeMaskSizeThenCorrectNodeMaskSizeIsReturned) {
    size_t sizeReturned = std::numeric_limits<size_t>::max();
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_NODE_MASK_KHR, 0, nullptr, &sizeReturned);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(sizeReturned, sizeof(cl_uint));
}

TEST(GetDeviceInfo, givenClDeviceWhenGettingDeviceInfoThenNodeMaskIsUnchanged) {
    cl_uint nodeMask = 0x1234u;
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_NODE_MASK_KHR, sizeof(cl_uint), &nodeMask, 0);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(nodeMask, 0x1234u);
}
} // namespace ULT
