/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

class GlobalBindlessWddmMemManagerFixture {
  public:
    struct FrontWindowMemManagerMock : public MockWddmMemoryManager {
        using MemoryManager::allocate32BitGraphicsMemoryImpl;

        FrontWindowMemManagerMock(NEO::ExecutionEnvironment &executionEnvironment) : MockWddmMemoryManager(executionEnvironment) {}
        FrontWindowMemManagerMock(bool enable64kbPages, bool enableLocalMemory, NEO::ExecutionEnvironment &executionEnvironment) : MockWddmMemoryManager(enable64kbPages, enableLocalMemory, executionEnvironment) {}
    };

    void setUp() {

        debugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        DeviceFactory::prepareDeviceEnvironments(*executionEnvironment);
        auto wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
        wddm->callBaseMapGpuVa = false;
        memManager = std::unique_ptr<FrontWindowMemManagerMock>(new FrontWindowMemManagerMock(*executionEnvironment));
    }
    void tearDown() {
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<FrontWindowMemManagerMock> memManager;
    DebugManagerStateRestore dbgRestorer;
};

using WddmGlobalBindlessAllocatorTests = Test<GlobalBindlessWddmMemManagerFixture>;

TEST_F(WddmGlobalBindlessAllocatorTests, givenAllocateInFrontWindowPoolFlagWhenWddmAllocate32BitGraphicsMemoryThenAllocateAtHeapBegining) {
    AllocationProperties allocationProperties{0u, MemoryConstants::kiloByte, NEO::AllocationType::BUFFER, {}};
    NEO::AllocationData allocData = {};
    allocData.rootDeviceIndex = allocationProperties.rootDeviceIndex;
    allocData.type = allocationProperties.allocationType;
    allocData.size = allocationProperties.size;
    allocData.allocationMethod = memManager->getPreferredAllocationMethod(allocationProperties);
    EXPECT_FALSE(GraphicsAllocation::isLockable(allocData.type));
    allocData.flags.use32BitFrontWindow = true;
    auto allocation = memManager->allocate32BitGraphicsMemoryImpl(allocData);
    auto gmmHelper = memManager->getGmmHelper(allocData.rootDeviceIndex);
    EXPECT_EQ(allocation->getGpuBaseAddress(), gmmHelper->canonize(allocation->getGpuAddress()));

    if (allocData.allocationMethod == GfxMemoryAllocationMethod::AllocateByKmd) {
        EXPECT_TRUE(allocation->isAllocationLockable());
    } else {
        EXPECT_FALSE(allocation->isAllocationLockable());
    }
    memManager->freeGraphicsMemory(allocation);
}

TEST_F(WddmGlobalBindlessAllocatorTests, givenLocalMemoryWhenSurfaceStatesAllocationCreatedThenGpuBaseAddressIsSetToCorrectBaseAddress) {
    MockAllocationProperties properties(0, true, MemoryConstants::pageSize64k, AllocationType::LINEAR_STREAM);
    properties.flags.use32BitFrontWindow = true;

    memManager.reset(new FrontWindowMemManagerMock(true, true, *executionEnvironment));

    executionEnvironment->rootDeviceEnvironments[0]->createBindlessHeapsHelper(memManager.get(), false, 0, 1);

    auto allocation = memManager->allocateGraphicsMemoryInPreferredPool(properties, nullptr);
    ASSERT_NE(nullptr, allocation);
    auto gmmHelper = memManager->getGmmHelper(0);
    EXPECT_EQ(gmmHelper->canonize(memManager->getExternalHeapBaseAddress(allocation->getRootDeviceIndex(), allocation->isAllocatedInLocalMemoryPool())), allocation->getGpuBaseAddress());

    ASSERT_NE(nullptr, executionEnvironment->rootDeviceEnvironments[0]->getBindlessHeapsHelper());
    EXPECT_EQ(executionEnvironment->rootDeviceEnvironments[0]->getBindlessHeapsHelper()->getGlobalHeapsBase(), allocation->getGpuBaseAddress());

    memManager->freeGraphicsMemory(allocation);
    executionEnvironment->rootDeviceEnvironments[0]->bindlessHeapsHelper.reset();
}

TEST_F(WddmGlobalBindlessAllocatorTests, givenLocalMemoryWhenSurfaceStatesAllocationCreatedInDevicePoolThenGpuBaseAddressIsSetToBindlessBaseAddress) {
    debugManager.flags.ForceLocalMemoryAccessMode.set(0);
    AllocationData allocData = {};
    allocData.type = AllocationType::LINEAR_STREAM;
    allocData.size = MemoryConstants::pageSize64k;

    auto wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
    wddm->callBaseMapGpuVa = true;

    memManager.reset(new FrontWindowMemManagerMock(true, true, *executionEnvironment));

    executionEnvironment->rootDeviceEnvironments[0]->createBindlessHeapsHelper(memManager.get(), false, 0, 1);

    MemoryManager::AllocationStatus status;
    auto allocation = memManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    auto gmmHelper = memManager->getGmmHelper(0);
    EXPECT_EQ(gmmHelper->canonize(memManager->getExternalHeapBaseAddress(allocation->getRootDeviceIndex(), true)), allocation->getGpuBaseAddress());

    ASSERT_NE(nullptr, executionEnvironment->rootDeviceEnvironments[0]->getBindlessHeapsHelper());
    EXPECT_EQ(executionEnvironment->rootDeviceEnvironments[0]->getBindlessHeapsHelper()->getGlobalHeapsBase(), allocation->getGpuBaseAddress());

    memManager->freeGraphicsMemory(allocation);
    executionEnvironment->rootDeviceEnvironments[0]->bindlessHeapsHelper.reset();
}

TEST_F(WddmGlobalBindlessAllocatorTests, givenLocalMemoryWhenSpecialSshHeapCreatedInDevicePoolThenGpuAddressIsSetToBindlessBaseAddress) {
    debugManager.flags.ForceLocalMemoryAccessMode.set(0);
    AllocationData allocData = {};
    allocData.type = AllocationType::LINEAR_STREAM;
    allocData.size = MemoryConstants::pageSize64k;

    auto wddm = static_cast<WddmMock *>(executionEnvironment->rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<Wddm>());
    wddm->callBaseMapGpuVa = true;

    memManager.reset(new FrontWindowMemManagerMock(true, true, *executionEnvironment));

    executionEnvironment->rootDeviceEnvironments[0]->createBindlessHeapsHelper(memManager.get(), false, 0, 1);

    auto gmmHelper = memManager->getGmmHelper(0);
    EXPECT_EQ(gmmHelper->canonize(memManager->getExternalHeapBaseAddress(0, true)),
              executionEnvironment->rootDeviceEnvironments[0]->getBindlessHeapsHelper()->getGlobalHeapsBase());

    EXPECT_EQ(executionEnvironment->rootDeviceEnvironments[0]->getBindlessHeapsHelper()->getGlobalHeapsBase(),
              executionEnvironment->rootDeviceEnvironments[0]->getBindlessHeapsHelper()->getHeap(BindlessHeapsHelper::BindlesHeapType::SPECIAL_SSH)->getGraphicsAllocation()->getGpuAddress());

    executionEnvironment->rootDeviceEnvironments[0]->bindlessHeapsHelper.reset();
}

TEST_F(WddmGlobalBindlessAllocatorTests, givenLocalMemoryWhenSurfaceStatesAllocationCreatedInPreferredPoolThenGpuBaseAddressIsSetToCorrectBaseAddress) {
    AllocationData allocData = {};
    allocData.type = AllocationType::LINEAR_STREAM;
    allocData.size = MemoryConstants::pageSize64k;

    memManager.reset(new FrontWindowMemManagerMock(true, true, *executionEnvironment));

    executionEnvironment->rootDeviceEnvironments[0]->createBindlessHeapsHelper(memManager.get(), false, 0, 1);

    AllocationProperties properties = {0, MemoryConstants::pageSize64k, AllocationType::LINEAR_STREAM, {}};
    auto allocation = memManager->allocateGraphicsMemoryInPreferredPool(properties, nullptr);
    ASSERT_NE(nullptr, allocation);
    auto gmmHelper = memManager->getGmmHelper(0);
    bool deviceMemory = allocation->isAllocatedInLocalMemoryPool();
    EXPECT_EQ(gmmHelper->canonize(memManager->getExternalHeapBaseAddress(allocation->getRootDeviceIndex(), deviceMemory)), allocation->getGpuBaseAddress());

    ASSERT_NE(nullptr, executionEnvironment->rootDeviceEnvironments[0]->getBindlessHeapsHelper());
    EXPECT_EQ(executionEnvironment->rootDeviceEnvironments[0]->getBindlessHeapsHelper()->getGlobalHeapsBase(), allocation->getGpuBaseAddress());

    memManager->freeGraphicsMemory(allocation);
    executionEnvironment->rootDeviceEnvironments[0]->bindlessHeapsHelper.reset();
}
} // namespace NEO
