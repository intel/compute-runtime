/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/mocks/mock_ostime_win.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

namespace ULT {

typedef ::testing::Test MockOSTimeWinTest;

TEST_F(MockOSTimeWinTest, whenCreatingTimerThenResolutionIsSetCorrectly) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddmMock = new WddmMock(rootDeviceEnvironment);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    wddmMock->init();

    wddmMock->timestampFrequency = 1000;

    std::unique_ptr<MockOSTimeWin> timeWin(new MockOSTimeWin(wddmMock));

    double res = 0.0;
    res = timeWin->getDynamicDeviceTimerResolution(device->getHardwareInfo());
    EXPECT_EQ(res, 1e+06);
}

TEST(GetDeviceInfo, givenClDeviceWhenGettingDeviceInfoThenCorrectAdapterLuidIsReturned) {
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    auto mockWddm = new WddmMock(*clDevice->executionEnvironment->rootDeviceEnvironments[0]);
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));

    std::array<uint8_t, CL_LUID_SIZE_KHR> deviceLuidKHR;
    auto luid = clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>()->getAdapterLuid();
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_LUID_KHR, CL_LUID_SIZE_KHR, deviceLuidKHR.data(), 0);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(std::memcmp(deviceLuidKHR.data(), &luid, CL_LUID_SIZE_KHR), 0);
}

TEST(GetDeviceInfo, givenClDeviceWhenVerifyAdapterLuidThenGetTrue) {
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    auto mockWddm = new WddmMock(*clDevice->executionEnvironment->rootDeviceEnvironments[0]);
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));

    cl_bool isValid = false;
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_LUID_VALID_KHR, sizeof(cl_bool), &isValid, 0);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_TRUE(isValid);
}

TEST(GetDeviceInfo, givenClDeviceWhenGettingDeviceInfoThenGetNodeMask) {
    auto clDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    auto mockWddm = new WddmMock(*clDevice->executionEnvironment->rootDeviceEnvironments[0]);
    clDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));

    cl_uint nodeMask = 0b0;
    auto retVal = clDevice->getDeviceInfo(CL_DEVICE_NODE_MASK_KHR, sizeof(cl_uint), &nodeMask, 0);

    ASSERT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(nodeMask, 0b1);
}
} // namespace ULT
