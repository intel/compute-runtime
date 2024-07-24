/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/linux/device_factory_tests_linux.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/os_interface/linux/sys_calls_linux_ult.h"

namespace NEO {
extern std::map<std::string, std::vector<std::string>> directoryFilesMap;
};

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

TEST(SortAndFilterDevicesDrmTest, whenSortingAndFilteringDevicesThenMemoryOperationHandlersHaveProperIndices) {
    static const auto numRootDevices = 6;
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.ZE_AFFINITY_MASK.set("1,2,3,4,5");

    VariableBackup<uint32_t> osContextCountBackup(&MemoryManager::maxOsContextCount);
    VariableBackup<std::map<std::string, std::vector<std::string>>> directoryFilesMapBackup(&directoryFilesMap);
    VariableBackup<const char *> pciDevicesDirectoryBackup(&Os::pciDevicesDirectory);
    VariableBackup<decltype(SysCalls::sysCallsOpen)> mockOpen(&SysCalls::sysCallsOpen, [](const char *pathname, int flags) -> int {
        return SysCalls::fakeFileDescriptor;
    });

    Os::pciDevicesDirectory = "/";
    directoryFilesMap.clear();
    directoryFilesMap[Os::pciDevicesDirectory] = {};
    directoryFilesMap[Os::pciDevicesDirectory].push_back("/pci-0003:01:02.1-render");
    directoryFilesMap[Os::pciDevicesDirectory].push_back("/pci-0000:00:02.0-render");
    directoryFilesMap[Os::pciDevicesDirectory].push_back("/pci-0000:01:03.0-render");
    directoryFilesMap[Os::pciDevicesDirectory].push_back("/pci-0000:01:02.1-render");
    directoryFilesMap[Os::pciDevicesDirectory].push_back("/pci-0000:00:02.1-render");
    directoryFilesMap[Os::pciDevicesDirectory].push_back("/pci-0003:01:02.0-render");

    ExecutionEnvironment executionEnvironment{};
    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_TRUE(success);

    static const auto newNumRootDevices = 5u;
    EXPECT_EQ(newNumRootDevices, executionEnvironment.rootDeviceEnvironments.size());

    NEO::PhysicalDevicePciBusInfo expectedBusInfos[newNumRootDevices] = {{0, 0, 2, 1}, {0, 1, 2, 1}, {0, 1, 3, 0}, {3, 1, 2, 0}, {3, 1, 2, 1}};

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < newNumRootDevices; rootDeviceIndex++) {
        auto pciBusInfo = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->getPciBusInfo();
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciDomain, pciBusInfo.pciDomain);
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciBus, pciBusInfo.pciBus);
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciDevice, pciBusInfo.pciDevice);
        EXPECT_EQ(expectedBusInfos[rootDeviceIndex].pciFunction, pciBusInfo.pciFunction);
        EXPECT_EQ(rootDeviceIndex, static_cast<DrmMemoryOperationsHandlerBind &>(*executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface).getRootDeviceIndex());
    }
}

TEST(DeviceFactoryAffinityMaskTest, whenAffinityMaskDoesNotSelectAnyDeviceThenEmptyEnvironmentIsReturned) {
    static const auto numRootDevices = 6;
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.ZE_AFFINITY_MASK.set("100");

    VariableBackup<uint32_t> osContextCountBackup(&MemoryManager::maxOsContextCount);
    ExecutionEnvironment executionEnvironment{};
    bool success = DeviceFactory::prepareDeviceEnvironments(executionEnvironment);
    EXPECT_TRUE(success);

    EXPECT_EQ(0u, executionEnvironment.rootDeviceEnvironments.size());
}
