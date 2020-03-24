/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/linux/device_factory_tests.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/os_interface.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"

TEST_F(DeviceFactoryLinuxTest, PrepareDeviceEnvironmentsCheckEUCntSSCnt) {
    const HardwareInfo *refHwinfo = defaultHwInfo.get();

    pDrm->StoredEUVal = 11;
    pDrm->StoredSSVal = 8;

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    auto hwInfo = executionEnvironment.rootDeviceEnvironments[0]->getHardwareInfo();

    EXPECT_TRUE(success);
    EXPECT_NE(hwInfo, nullptr);
    EXPECT_EQ(refHwinfo->platform.eDisplayCoreFamily, hwInfo->platform.eDisplayCoreFamily);
    EXPECT_EQ((int)hwInfo->gtSystemInfo.EUCount, 11);
    EXPECT_EQ((int)hwInfo->gtSystemInfo.SubSliceCount, 8);

    //temporararily return GT2.
    EXPECT_EQ(1u, hwInfo->featureTable.ftrGT2);
}

TEST_F(DeviceFactoryLinuxTest, PrepareDeviceEnvironmentsDrmCreateFailedConfigureHwInfo) {

    pDrm->StoredRetValForDeviceID = -1;

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_FALSE(success);

    pDrm->StoredRetValForDeviceID = 0;
}

TEST_F(DeviceFactoryLinuxTest, givenGetDeviceCallWhenItIsDoneThenOsInterfaceIsAllocatedAndItContainDrm) {
    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_TRUE(success);
    EXPECT_NE(nullptr, executionEnvironment.rootDeviceEnvironments[0]->osInterface);
    EXPECT_NE(nullptr, pDrm);
    EXPECT_EQ(pDrm, executionEnvironment.rootDeviceEnvironments[0]->osInterface->get()->getDrm());
}

TEST_F(DeviceFactoryLinuxTest, whenDrmIsNotCretedThenPrepareDeviceEnvironmentsFails) {
    delete pDrm;
    pDrm = nullptr;

    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_FALSE(success);
}
