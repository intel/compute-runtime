/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/unit_test/fixtures/device_fixture.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "test.h"

namespace NEO {

class MemManagerFixture : public DeviceFixture {
  public:
    struct FrontWindowMemManagerMock : public MockMemoryManager {
        FrontWindowMemManagerMock(NEO::ExecutionEnvironment &executionEnvironment) : MockMemoryManager(executionEnvironment) {}
        void forceLimitedRangeAllocator(uint32_t rootDeviceIndex, uint64_t range) { getGfxPartition(rootDeviceIndex)->init(range, 0, 0, gfxPartitions.size(), true); }
    };

    void SetUp() {
        DebugManagerStateRestore dbgRestorer;
        DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
        DeviceFixture::SetUp();
        memManager = std::unique_ptr<FrontWindowMemManagerMock>(new FrontWindowMemManagerMock(*pDevice->getExecutionEnvironment()));
    }
    void TearDown() {
        DeviceFixture::TearDown();
    }
    std::unique_ptr<FrontWindowMemManagerMock> memManager;
};

using FrontWindowAllocatorTests = Test<MemManagerFixture>;

TEST_F(FrontWindowAllocatorTests, givenAllocateInFrontWindowPoolFlagWhenAllocate32BitGraphicsMemoryThenAllocateAtHeapBegining) {
    AllocationData allocData = {};
    allocData.flags.use32BitFrontWindow = true;
    allocData.size = MemoryConstants::kiloByte;
    auto allocation(memManager->allocate32BitGraphicsMemoryImpl(allocData, false));
    EXPECT_EQ(allocation->getGpuBaseAddress(), allocation->getGpuAddress());
    memManager->freeGraphicsMemory(allocation);
}

TEST_F(FrontWindowAllocatorTests, givenAllocateInFrontWindowPoolFlagWhenAllocate32BitGraphicsMemoryThenAlocationInFrontWindowPoolRange) {
    AllocationData allocData = {};
    allocData.flags.use32BitFrontWindow = true;
    allocData.size = MemoryConstants::kiloByte;
    auto allocation(memManager->allocate32BitGraphicsMemoryImpl(allocData, false));
    auto heap = memManager->heapAssigner.get32BitHeapIndex(allocData.type, false, *defaultHwInfo, true);
    EXPECT_EQ(GmmHelper::canonize(memManager->getGfxPartition(0)->getHeapMinimalAddress(heap)), allocation->getGpuAddress());
    EXPECT_LT(ptrOffset(allocation->getGpuAddress(), allocation->getUnderlyingBufferSize()), GmmHelper::canonize(memManager->getGfxPartition(0)->getHeapLimit(heap)));
    memManager->freeGraphicsMemory(allocation);
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenExternalHeapHaveFrontWindowPool) {
    EXPECT_NE(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW), 0u);
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenFrontWindowPoolIsAtBeginingOfExternalHeap) {
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW));
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW));
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenMinimalAdressIsNotAtBeginingOfExternalHeap) {
    EXPECT_GT(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::HEAP_EXTERNAL), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL));
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenMinimalAdressIsAtBeginingOfExternalFrontWindowHeap) {
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW));
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshEnabledThenMinimalAdressIsAtBeginingOfExternalDeviceFrontWindowHeap) {
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::HEAP_EXTERNAL_DEVICE_FRONT_WINDOW), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL_DEVICE_FRONT_WINDOW));
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshDisabledThenMinimalAdressEqualBeginingOfExternalHeap) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(false);
    memManager.reset(new FrontWindowMemManagerMock(*pDevice->getExecutionEnvironment()));
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapMinimalAddress(HeapIndex::HEAP_EXTERNAL), memManager->getGfxPartition(0)->getHeapBase(HeapIndex::HEAP_EXTERNAL) + GfxPartition::heapGranularity);
}

TEST_F(FrontWindowAllocatorTests, givenInitializedHeapsWhenUseExternalAllocatorForSshAndDshDisabledThenExternalHeapDoesntHaveFrontWindowPool) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(false);
    memManager.reset(new FrontWindowMemManagerMock(*pDevice->getExecutionEnvironment()));
    EXPECT_EQ(memManager->getGfxPartition(0)->getHeapLimit(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW), 0u);
}

TEST_F(FrontWindowAllocatorTests, givenLinearStreamAllocWhenSelectingHeapWithFrontWindowThenCorrectIndexReturned) {
    GraphicsAllocation allocation{0, GraphicsAllocation::AllocationType::LINEAR_STREAM, nullptr, 0, 0, 0, MemoryPool::MemoryNull};
    EXPECT_EQ(HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW, memManager->selectHeap(&allocation, true, true, true));
}

} // namespace NEO