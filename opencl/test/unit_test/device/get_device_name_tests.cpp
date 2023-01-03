/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "gtest/gtest.h"

using namespace NEO;

using DeviceNameTest = ::testing::Test;

TEST_F(DeviceNameTest, WhenCallingGetClDeviceNameThenReturnDeviceNameWithDeviceIdAppendedAtTheEnd) {
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    std::string deviceName = "Intel(R) Graphics";
    EXPECT_STREQ(deviceName.c_str(), clDevice->device.getDeviceName(*defaultHwInfo.get()).c_str());

    std::stringstream clDeviceName;
    clDeviceName << deviceName;
    clDeviceName << " [0x" << std::hex << std::setw(4) << std::setfill('0') << defaultHwInfo->platform.usDeviceID << "]";
    EXPECT_STREQ(clDeviceName.str().c_str(), clDevice->getClDeviceName(*defaultHwInfo.get()).c_str());
}

TEST_F(DeviceNameTest, GivenDeviceWithNameWhenCallingGetClDeviceNameThenReturnCustomDeviceName) {
    HardwareInfo localHwInfo = *defaultHwInfo;
    localHwInfo.capabilityTable.deviceName = "Custom Device";

    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));

    std::string deviceName = "Custom Device";
    EXPECT_STREQ(deviceName.c_str(), clDevice->device.getDeviceName(localHwInfo).c_str());
}
