/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/front_window_fixture.h"

using namespace NEO;

TEST(BindlessHeapsHelper, givenExternalAllocatorFlagEnabledWhenCreatingRootDevicesThenBindlesHeapHelperCreated) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    EXPECT_NE(deviceFactory->rootDevices[0]->getBindlessHeapsHelper(), nullptr);
}

TEST(BindlessHeapsHelper, givenExternalAllocatorFlagDisabledWhenCreatingRootDevicesThenBindlesHeapHelperIsNotCreated) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(0);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    EXPECT_EQ(deviceFactory->rootDevices[0]->getBindlessHeapsHelper(), nullptr);
}

class BindlessHeapsHelperFixture {
  public:
    void setUp() {

        DebugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
        memManager = std::make_unique<MockMemoryManager>();
    }
    void tearDown() {}

    MemoryManager *getMemoryManager() {
        return memManager.get();
    }

    DebugManagerStateRestore dbgRestorer;
    std::unique_ptr<MockMemoryManager> memManager;
    uint32_t rootDeviceIndex = 0;
    DeviceBitfield devBitfield = 1;
    int genericSubDevices = 1;
};

using BindlessHeapsHelperTests = Test<BindlessHeapsHelperFixture>;

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenItsCreatedThenSpecialSshAllocatedAtHeapBegining) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    auto specialSshAllocation = bindlessHeapHelper->specialSsh->getGraphicsAllocation();
    EXPECT_EQ(specialSshAllocation->getGpuAddress(), specialSshAllocation->getGpuBaseAddress());
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapThenHeapUsedSpaceGrow) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    auto usedBefore = bindlessHeapHelper->globalSsh->getUsed();

    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    auto usedAfter = bindlessHeapHelper->globalSsh->getUsed();
    EXPECT_GT(usedAfter, usedBefore);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapThenMemoryAtReturnedOffsetIsCorrect) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);

    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);

    auto allocInHeapPtr = bindlessHeapHelper->globalSsh->getGraphicsAllocation()->getUnderlyingBuffer();
    EXPECT_EQ(ssInHeapInfo.ssPtr, allocInHeapPtr);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapTwiceForTheSameAllocationThenTheSameOffsetReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);

    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo1 = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    auto ssInHeapInfo2 = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);

    EXPECT_EQ(ssInHeapInfo1.surfaceStateOffset, ssInHeapInfo2.surfaceStateOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapTwiceForDifferentAllocationThenDifferentOffsetsReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);

    MockGraphicsAllocation alloc1;
    MockGraphicsAllocation alloc2;
    size_t size = 0x40;
    auto ss = std::make_unique<uint8_t[]>(size);
    memset(ss.get(), 35, size);
    auto ssInHeapInfo1 = bindlessHeapHelper->allocateSSInHeap(size, &alloc1, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    auto ssInHeapInfo2 = bindlessHeapHelper->allocateSSInHeap(size, &alloc2, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);

    EXPECT_NE(ssInHeapInfo1.surfaceStateOffset, ssInHeapInfo2.surfaceStateOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocatingMoreSsThenNewHeapAllocationCreated) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    size_t ssSize = 0x40;
    auto ssCount = bindlessHeapHelper->globalSsh->getAvailableSpace() / ssSize;
    auto graphicsAllocations = std::make_unique<MockGraphicsAllocation[]>(ssCount);
    auto ssAllocationBefore = bindlessHeapHelper->globalSsh->getGraphicsAllocation();
    for (uint32_t i = 0; i < ssCount; i++) {
        bindlessHeapHelper->allocateSSInHeap(ssSize, &graphicsAllocations[i], BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    }
    MockGraphicsAllocation alloc;
    bindlessHeapHelper->allocateSSInHeap(ssSize, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);

    auto ssAllocationAfter = bindlessHeapHelper->globalSsh->getGraphicsAllocation();

    EXPECT_NE(ssAllocationBefore, ssAllocationAfter);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenCreatedThenAllocationsHaveTheSameBaseAddress) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    for (auto allocation : bindlessHeapHelper->ssHeapsAllocations) {
        EXPECT_EQ(allocation->getGpuBaseAddress(), bindlessHeapHelper->getGlobalHeapsBase());
    }
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenGetDefaultBorderColorOffsetCalledThenCorrectOffsetReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    auto expectedOffset = bindlessHeapHelper->borderColorStates->getGpuAddress() - bindlessHeapHelper->borderColorStates->getGpuBaseAddress();
    EXPECT_EQ(bindlessHeapHelper->getDefaultBorderColorOffset(), expectedOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenGetAlphaBorderColorOffsetCalledThenCorrectOffsetReturned) {
    auto borderColorSize = 0x40;
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    auto expectedOffset = bindlessHeapHelper->borderColorStates->getGpuAddress() - bindlessHeapHelper->borderColorStates->getGpuBaseAddress() + borderColorSize;
    EXPECT_EQ(bindlessHeapHelper->getAlphaBorderColorOffset(), expectedOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInSpecialHeapThenOffsetLessThanFrontWindowSize) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::SPECIAL_SSH);
    auto frontWindowSize = GfxPartition::externalFrontWindowPoolSize;
    EXPECT_LT(ssInHeapInfo.surfaceStateOffset, frontWindowSize);
}
TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInGlobalHeapThenOffsetLessThanFrontWindowSize) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    auto frontWindowSize = GfxPartition::externalFrontWindowPoolSize;
    EXPECT_LT(ssInHeapInfo.surfaceStateOffset, frontWindowSize);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInScratchHeapThenOffsetLessThanFrontWindowSize) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::SCRATCH_SSH);
    auto frontWindowSize = GfxPartition::externalFrontWindowPoolSize;
    EXPECT_LT(ssInHeapInfo.surfaceStateOffset, frontWindowSize);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInGlobalDshThenOffsetGreaterOrEqualFrontWindowSize) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_DSH);
    auto frontWindowSize = GfxPartition::externalFrontWindowPoolSize;
    EXPECT_GE(ssInHeapInfo.surfaceStateOffset, frontWindowSize);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenFreeGraphicsMemoryIsCalledThenSSinHeapInfoShouldBePlacedInReuseVector) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    MockBindlesHeapsHelper *bindlessHeapHelperPtr = bindlessHeapHelper.get();
    memManager->mockExecutionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->bindlessHeapsHelper.reset(bindlessHeapHelper.release());
    MockGraphicsAllocation *alloc = new MockGraphicsAllocation;
    size_t size = 0x40;
    auto ssinHeapInfo = bindlessHeapHelperPtr->allocateSSInHeap(size, alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    memManager->freeGraphicsMemory(alloc);
    EXPECT_EQ(bindlessHeapHelperPtr->surfaceStateInHeapVectorReuse.size(), 1u);
    auto ssInHeapInfoFromReuseVector = bindlessHeapHelperPtr->surfaceStateInHeapVectorReuse.front().get();
    EXPECT_EQ(ssInHeapInfoFromReuseVector->surfaceStateOffset, ssinHeapInfo.surfaceStateOffset);
    EXPECT_EQ(ssInHeapInfoFromReuseVector->ssPtr, ssinHeapInfo.ssPtr);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperPreviousAllocationThenItShouldBeReused) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    MockBindlesHeapsHelper *bindlessHeapHelperPtr = bindlessHeapHelper.get();
    MockGraphicsAllocation *alloc = new MockGraphicsAllocation;
    memManager->mockExecutionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->bindlessHeapsHelper.reset(bindlessHeapHelper.release());
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelperPtr->allocateSSInHeap(size, alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    memManager->freeGraphicsMemory(alloc);
    EXPECT_EQ(bindlessHeapHelperPtr->surfaceStateInHeapVectorReuse.size(), 1u);
    MockGraphicsAllocation *alloc2 = new MockGraphicsAllocation;
    auto reusedSSinHeapInfo = bindlessHeapHelperPtr->allocateSSInHeap(size, alloc2, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    EXPECT_EQ(bindlessHeapHelperPtr->surfaceStateInHeapVectorReuse.size(), 0u);
    EXPECT_EQ(ssInHeapInfo.surfaceStateOffset, reusedSSinHeapInfo.surfaceStateOffset);
    EXPECT_EQ(ssInHeapInfo.ssPtr, reusedSSinHeapInfo.ssPtr);
    memManager->freeGraphicsMemory(alloc2);
}
TEST_F(BindlessHeapsHelperTests, givenDeviceWhenBindlessHeapHelperInitializedThenCorrectDeviceBitFieldIsUsed) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    DeviceBitfield deviceBitfield = 7;
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, deviceBitfield);
    EXPECT_EQ(reinterpret_cast<MockMemoryManager *>(getMemoryManager())->recentlyPassedDeviceBitfield, deviceBitfield);
}
