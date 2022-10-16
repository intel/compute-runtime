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
