/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/linux/device_factory_tests_linux.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"

TEST_F(DeviceFactoryLinuxTest, WhenPreparingDeviceEnvironmentsThenInitializedCorrectly) {
    const HardwareInfo *refHwinfo = defaultHwInfo.get();

    pDrm->storedEUVal = 16;
    pDrm->storedSSVal = 8;

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();

    EXPECT_TRUE(success);
    EXPECT_NE(hwInfo, nullptr);
    EXPECT_EQ(refHwinfo->platform.eDisplayCoreFamily, hwInfo->platform.eDisplayCoreFamily);
    EXPECT_EQ((int)hwInfo->gtSystemInfo.EUCount, 16);
    EXPECT_EQ((int)hwInfo->gtSystemInfo.SubSliceCount, 8);
    EXPECT_EQ((int)hwInfo->gtSystemInfo.DualSubSliceCount, 8);
}

TEST_F(DeviceFactoryLinuxTest, givenSomeDisabledSSAndEUWhenPrepareDeviceEnvironmentsThenCorrectObtainEUCntSSCnt) {
    const HardwareInfo *refHwinfo = defaultHwInfo.get();

    pDrm->storedEUVal = 144;
    pDrm->storedSSVal = 12;
    pDrm->storedSVal = 2;
    pDrm->disableSomeTopology = true;

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();

    EXPECT_TRUE(success);
    EXPECT_NE(hwInfo, nullptr);
    EXPECT_EQ(refHwinfo->platform.eDisplayCoreFamily, hwInfo->platform.eDisplayCoreFamily);
    EXPECT_EQ((int)hwInfo->gtSystemInfo.SliceCount, 1);
    EXPECT_EQ((int)hwInfo->gtSystemInfo.SubSliceCount, 2);
    EXPECT_EQ((int)hwInfo->gtSystemInfo.DualSubSliceCount, 2);
    EXPECT_EQ((int)hwInfo->gtSystemInfo.EUCount, 12);
}

TEST_F(DeviceFactoryLinuxTest, GivenInvalidHwInfoWhenPreparingDeviceEnvironmentsThenFailIsReturned) {

    pDrm->storedRetValForDeviceID = -1;

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_FALSE(success);

    pDrm->storedRetValForDeviceID = 0;
}

TEST_F(DeviceFactoryLinuxTest, givenGetDeviceCallWhenItIsDoneThenOsInterfaceIsAllocatedAndItContainDrm) {
    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_NE(nullptr, executionEnvironment.rootDeviceEnvironments[0]->osInterface);
    EXPECT_NE(nullptr, pDrm);
    EXPECT_EQ(pDrm, executionEnvironment.rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Drm>());
}

TEST_F(DeviceFactoryLinuxTest, whenDrmIsNotCretedThenPrepareDeviceEnvironmentsFails) {
    delete pDrm;
    pDrm = nullptr;

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_FALSE(success);
}
