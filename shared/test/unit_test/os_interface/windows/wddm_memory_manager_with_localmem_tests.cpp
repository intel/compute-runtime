/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(WddmMemoryManagerWithLocalMemoryTest, whenAllocatingKernelIsaWithSpecificGpuAddressThenThisAddressIsUsed) {
    if (is32bit) {
        GTEST_SKIP();
    }
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    ultHwConfig.forceOsAgnosticMemoryManager = false;

    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableLocalMemory.set(true);

    auto executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), true, 1);
    auto devices = DeviceFactory::createDevices(*executionEnvironment);
    auto memoryManager = executionEnvironment->memoryManager.get();
    auto &device = devices.front();

    uint64_t expectedGpuAddress = memoryManager->getInternalHeapBaseAddress(device->getRootDeviceIndex(), true) + MemoryConstants::gigaByte;
    size_t size = 4096u;

    AllocationProperties properties(device->getRootDeviceIndex(), true, size, AllocationType::KERNEL_ISA, false, device->getDeviceBitfield());
    properties.gpuAddress = expectedGpuAddress;

    auto graphicsAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
    ASSERT_NE(nullptr, graphicsAllocation);

    EXPECT_EQ(device->getGmmHelper()->canonize(expectedGpuAddress), graphicsAllocation->getGpuAddress());
    EXPECT_EQ(MemoryPool::LocalMemory, graphicsAllocation->getMemoryPool());
    memoryManager->freeGraphicsMemory(graphicsAllocation);
}
