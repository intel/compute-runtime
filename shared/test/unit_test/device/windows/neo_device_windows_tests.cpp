/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_ostime_win.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/test_macros/test.h"

using DeviceTest = Test<DeviceFixture>;

TEST_F(DeviceTest, GivenDeviceWhenGetAdapterLuidThenLuidIsSet) {
    std::array<uint8_t, HwInfoConfig::luidSize> luid, expectedLuid;
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    auto mockWddm = new WddmMock(*pDevice->executionEnvironment->rootDeviceEnvironments[0]);
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));

    memcpy_s(expectedLuid.data(), HwInfoConfig::luidSize, &mockWddm->mockAdaperLuid, sizeof(LUID));
    pDevice->getAdapterLuid(luid);

    EXPECT_EQ(expectedLuid, luid);
}

TEST_F(DeviceTest, GivenDeviceWhenVerifyAdapterLuidThenTrueIsReturned) {
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    auto mockWddm = new WddmMock(*pDevice->executionEnvironment->rootDeviceEnvironments[0]);
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));

    EXPECT_TRUE(pDevice->verifyAdapterLuid());
}

TEST_F(DeviceTest, GivenFailingVerifyAdapterLuidWhenGetAdapterMaskThenMaskIsNotSet) {
    uint32_t nodeMask = 0x1234u;
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    auto mockWddm = new WddmMock(*pDevice->executionEnvironment->rootDeviceEnvironments[0]);
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));
    pDevice->callBaseVerifyAdapterLuid = false;
    pDevice->verifyAdapterLuidReturnValue = false;
    pDevice->getAdapterMask(nodeMask);

    EXPECT_EQ(nodeMask, 0x1234u);
}

TEST_F(DeviceTest, GivenDeviceWhenGetAdapterMaskThenMaskIsSet) {
    uint32_t nodeMask = 0x1234u;
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
    auto mockWddm = new WddmMock(*pDevice->executionEnvironment->rootDeviceEnvironments[0]);
    pDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(mockWddm));
    pDevice->getAdapterMask(nodeMask);

    EXPECT_EQ(nodeMask, 1u);
}

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

TEST_F(MockOSTimeWinTest, whenConvertingTimestampsToCsDomainThenTimestampDataAreSetCorrectly) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment rootDeviceEnvironment(executionEnvironment);
    auto wddmMock = new WddmMock(rootDeviceEnvironment);
    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    wddmMock->init();
    std::unique_ptr<MockOSTimeWin> timeWin(new MockOSTimeWin(wddmMock));
    uint64_t timestampData = 0x1234;
    uint64_t freqOA = 1;
    uint64_t freqCS = 0;
    auto expectedGpuTicksWhenEqualOne = timestampData;
    timeWin->convertTimestampsFromOaToCsDomain(timestampData, freqOA, freqCS);
    EXPECT_EQ(expectedGpuTicksWhenEqualOne, timestampData);
    freqCS = 1;
    timeWin->convertTimestampsFromOaToCsDomain(timestampData, freqOA, freqCS);
    EXPECT_EQ(expectedGpuTicksWhenEqualOne, timestampData);
    freqOA = 2;
    uint64_t ratio = freqOA / freqCS;
    auto expectedGpuTicksWhenRatioBiggerThanOne = timestampData / ratio;
    timeWin->convertTimestampsFromOaToCsDomain(timestampData, freqOA, freqCS);
    EXPECT_EQ(expectedGpuTicksWhenRatioBiggerThanOne, timestampData);
}