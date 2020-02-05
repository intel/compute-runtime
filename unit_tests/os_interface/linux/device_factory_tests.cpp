/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/linux/device_factory_tests.h"

#include "core/execution_environment/root_device_environment.h"
#include "core/os_interface/linux/os_interface.h"
#include "core/os_interface/os_interface.h"
#include "core/unit_tests/helpers/default_hw_info.h"

TEST_F(DeviceFactoryLinuxTest, GetDevicesCheckEUCntSSCnt) {
    const HardwareInfo *refHwinfo = *platformDevices;
    size_t numDevices = 0;

    pDrm->StoredEUVal = 11;
    pDrm->StoredSSVal = 8;

    bool success = DeviceFactory::getDevices(numDevices, executionEnvironment);
    auto hwInfo = executionEnvironment.getHardwareInfo();

    EXPECT_TRUE(success);
    EXPECT_EQ((int)numDevices, 1);
    EXPECT_NE(hwInfo, nullptr);
    EXPECT_EQ(refHwinfo->platform.eDisplayCoreFamily, hwInfo->platform.eDisplayCoreFamily);
    EXPECT_EQ((int)hwInfo->gtSystemInfo.EUCount, 11);
    EXPECT_EQ((int)hwInfo->gtSystemInfo.SubSliceCount, 8);

    //temporararily return GT2.
    EXPECT_EQ(1u, hwInfo->featureTable.ftrGT2);

    DeviceFactory::releaseDevices();
}

TEST_F(DeviceFactoryLinuxTest, GetDevicesDrmCreateFailed) {
    size_t numDevices = 0;

    pushDrmMock(nullptr);
    bool success = DeviceFactory::getDevices(numDevices, executionEnvironment);
    EXPECT_FALSE(success);

    popDrmMock();
}

TEST_F(DeviceFactoryLinuxTest, GetDevicesDrmCreateFailedConfigureHwInfo) {
    size_t numDevices = 0;

    pDrm->StoredRetValForDeviceID = -1;

    bool success = DeviceFactory::getDevices(numDevices, executionEnvironment);
    EXPECT_FALSE(success);

    pDrm->StoredRetValForDeviceID = 0;
}

TEST_F(DeviceFactoryLinuxTest, ReleaseDevices) {
    MockDeviceFactory mockDeviceFactory;
    size_t numDevices = 0;

    pDrm->StoredDeviceID = 0x5A84;
    pDrm->StoredDeviceRevID = 0x0B;
    pDrm->StoredEUVal = 18;
    pDrm->StoredSSVal = 3;
    pDrm->StoredHasPooledEU = 1;
    pDrm->StoredMinEUinPool = 9;
    pDrm->StoredRetVal = -1;

    bool success = mockDeviceFactory.getDevices(numDevices, executionEnvironment);
    EXPECT_TRUE(success);

    mockDeviceFactory.releaseDevices();
    EXPECT_TRUE(mockDeviceFactory.getNumDevices() == 0);
}

TEST_F(DeviceFactoryLinuxTest, givenGetDeviceCallWhenItIsDoneThenOsInterfaceIsAllocatedAndItContainDrm) {
    MockDeviceFactory mockDeviceFactory;
    size_t numDevices = 0;
    bool success = mockDeviceFactory.getDevices(numDevices, executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_NE(nullptr, executionEnvironment.rootDeviceEnvironments[0]->osInterface);
    EXPECT_NE(nullptr, pDrm);
    EXPECT_EQ(pDrm, executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->getDrm());
}
