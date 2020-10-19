/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/default_hw_info.h"
#include "shared/test/unit_test/mocks/mock_device.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "gtest/gtest.h"

namespace NEO {
extern const char *familyName[];
} // namespace NEO

using namespace NEO;

using DeviceNameTest = ::testing::Test;

TEST_F(DeviceNameTest, WhenCallingGetClDeviceNameThenReturnDeviceNameWithDeviceIdAppendedAtTheEnd) {
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));

    std::string deviceName = "Intel(R) Graphics";
    if (defaultHwInfo->platform.eProductFamily < PRODUCT_FAMILY::IGFX_DG1) {
        deviceName += std::string(" ") + familyName[defaultHwInfo->platform.eRenderCoreFamily];
    }
    EXPECT_STREQ(deviceName.c_str(), clDevice->device.getDeviceName(*defaultHwInfo.get()).c_str());

    std::stringstream clDeviceName;
    clDeviceName << deviceName;
    clDeviceName << " [0x" << std::hex << std::setw(4) << std::setfill('0') << defaultHwInfo->platform.usDeviceID << "]";
    EXPECT_STREQ(clDeviceName.str().c_str(), clDevice->getClDeviceName(*defaultHwInfo.get()).c_str());
}
