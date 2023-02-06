/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "gtest/gtest.h"

using namespace NEO;

using DeviceNameTest = ::testing::Test;

TEST_F(DeviceNameTest, WhenCallingGetClDeviceNameThenReturnDeviceNameFromBaseDevice) {
    {
        auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

        EXPECT_STREQ(clDevice->device.getDeviceName().c_str(), clDevice->getClDeviceName().c_str());
    }

    {
        HardwareInfo localHwInfo = *defaultHwInfo;
        localHwInfo.capabilityTable.deviceName = "Custom Device";

        auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&localHwInfo));
        EXPECT_STREQ(clDevice->device.getDeviceName().c_str(), clDevice->getClDeviceName().c_str());
    }
}
