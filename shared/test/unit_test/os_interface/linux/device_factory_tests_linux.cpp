/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/linux/device_factory_tests_linux.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_driver_model.h"

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

TEST(SortDevicesDrmTest, whenSortingDevicesThenMemoryOperationHandlersHaveProperIndices) {
    ExecutionEnvironment executionEnvironment{};
    static const auto numRootDevices = 6;
    NEO::PhysicalDevicePciBusInfo inputBusInfos[numRootDevices] = {{3, 1, 2, 1}, {0, 0, 2, 0}, {0, 1, 3, 0}, {0, 1, 2, 1}, {0, 0, 2, 1}, {3, 1, 2, 0}};
    NEO::PhysicalDevicePciBusInfo expectedBusInfos[numRootDevices] = {{0, 0, 2, 0}, {0, 0, 2, 1}, {0, 1, 2, 1}, {0, 1, 3, 0}, {3, 1, 2, 0}, {3, 1, 2, 1}};

    executionEnvironment.prepareRootDeviceEnvironments(numRootDevices);
    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex];
        auto osInterface = std::make_unique<OSInterface>();
        auto driverModel = std::make_unique<MockDriverModel>(DriverModelType::DRM);
        driverModel->pciBusInfo = inputBusInfos[rootDeviceIndex];
        osInterface->setDriverModel(std::move(driverModel));
        rootDeviceEnvironment.osInterface = std::move(osInterface);
        rootDeviceEnvironment.memoryOperationsInterface = std::make_unique<DrmMemoryOperationsHandlerBind>(rootDeviceEnvironment, rootDeviceIndex);
    }

    executionEnvironment.sortNeoDevices();

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < numRootDevices; rootDeviceIndex++) {
        auto pciBusInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->getPciBusInfo();
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciDomain, pciBusInfo.pciDomain);
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciBus, pciBusInfo.pciBus);
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciDevice, pciBusInfo.pciDevice);
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciFunction, pciBusInfo.pciFunction);
        EXPECT_EQ(rootDeviceIndex, static_cast<DrmMemoryOperationsHandlerBind &>(*executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface).getRootDeviceIndex());
    }
}
