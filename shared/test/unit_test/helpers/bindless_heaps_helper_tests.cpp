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

TEST(BindlessHeapsHelper, givenExternalAllocatorFlagAndBindlessModeEnabledWhenCreatingRootDevicesThenBindlessHeapsHelperCreated) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    debugManager.flags.UseBindlessMode.set(1);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    EXPECT_NE(deviceFactory->rootDevices[0]->getBindlessHeapsHelper(), nullptr);
}

TEST(BindlessHeapsHelper, givenExternalAllocatorFlagEnabledAndBindlessModeDisabledWhenCreatingRootDevicesThenBindlessHeapsHelperIsNotCreated) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    debugManager.flags.UseBindlessMode.set(0);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    EXPECT_EQ(deviceFactory->rootDevices[0]->getBindlessHeapsHelper(), nullptr);
}

TEST(BindlessHeapsHelper, givenExternalAllocatorFlagDisabledWhenCreatingRootDevicesThenBindlesHeapHelperIsNotCreated) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(0);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    EXPECT_EQ(deviceFactory->rootDevices[0]->getBindlessHeapsHelper(), nullptr);
}

class BindlessHeapsHelperFixture {
  public:
    void setUp() {

        debugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
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
    bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    auto usedAfter = bindlessHeapHelper->globalSsh->getUsed();
    EXPECT_GT(usedAfter, usedBefore);
}

TEST_F(BindlessHeapsHelperTests, givenNoMemoryAvailableWhenAllocateSsInHeapThenNullptrIsReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);

    MockGraphicsAllocation alloc;
    size_t size = 0x40;

    bindlessHeapHelper->globalSsh->getSpace(bindlessHeapHelper->globalSsh->getAvailableSpace());
    memManager->failAllocateSystemMemory = true;
    memManager->failAllocate32Bit = true;

    auto info = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    EXPECT_EQ(nullptr, info.ssPtr);
    EXPECT_EQ(nullptr, info.heapAllocation);
}

TEST_F(BindlessHeapsHelperTests, givenNoMemoryAvailableWhenAllocatingBindlessSlotThenFalseIsReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    auto bindlessHeapHelperPtr = bindlessHeapHelper.get();
    memManager->mockExecutionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->bindlessHeapsHelper.reset(bindlessHeapHelper.release());
    MockGraphicsAllocation alloc;

    bindlessHeapHelperPtr->globalSsh->getSpace(bindlessHeapHelperPtr->globalSsh->getAvailableSpace());
    memManager->failAllocateSystemMemory = true;
    memManager->failAllocate32Bit = true;

    EXPECT_FALSE(memManager->allocateBindlessSlot(&alloc));
    EXPECT_EQ(nullptr, alloc.getBindlessInfo().ssPtr);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapThenMemoryAtReturnedOffsetIsCorrect) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);

    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::globalSsh);

    auto allocInHeapPtr = bindlessHeapHelper->globalSsh->getGraphicsAllocation()->getUnderlyingBuffer();
    EXPECT_EQ(ssInHeapInfo.ssPtr, allocInHeapPtr);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapForImageThenTwoBindlessSlotsAreAllocated) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    auto surfaceStateSize = bindlessHeapHelper->surfaceStateSize;
    memManager->mockExecutionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->bindlessHeapsHelper.reset(bindlessHeapHelper.release());

    MockGraphicsAllocation alloc;
    alloc.allocationType = AllocationType::image;
    EXPECT_TRUE(getMemoryManager()->allocateBindlessSlot(&alloc));
    auto ssInHeapInfo1 = alloc.getBindlessInfo();

    EXPECT_EQ(surfaceStateSize * 2, ssInHeapInfo1.ssSize);

    MockGraphicsAllocation alloc2;
    alloc2.allocationType = AllocationType::sharedImage;
    EXPECT_TRUE(getMemoryManager()->allocateBindlessSlot(&alloc2));
    auto ssInHeapInfo2 = alloc2.getBindlessInfo();

    EXPECT_EQ(surfaceStateSize * 2, ssInHeapInfo2.ssSize);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapTwiceForTheSameAllocationThenTheSameOffsetReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);

    MockGraphicsAllocation alloc;

    EXPECT_TRUE(getMemoryManager()->allocateBindlessSlot(&alloc));
    auto ssInHeapInfo1 = alloc.getBindlessInfo();

    EXPECT_TRUE(getMemoryManager()->allocateBindlessSlot(&alloc));
    auto ssInHeapInfo2 = alloc.getBindlessInfo();
    EXPECT_EQ(ssInHeapInfo1.surfaceStateOffset, ssInHeapInfo2.surfaceStateOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapTwiceForDifferentAllocationsThenDifferentOffsetsReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);

    MockGraphicsAllocation alloc1;
    MockGraphicsAllocation alloc2;
    size_t size = 0x40;
    auto ss = std::make_unique<uint8_t[]>(size);
    memset(ss.get(), 35, size);
    auto ssInHeapInfo1 = bindlessHeapHelper->allocateSSInHeap(size, &alloc1, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    auto ssInHeapInfo2 = bindlessHeapHelper->allocateSSInHeap(size, &alloc2, BindlessHeapsHelper::BindlesHeapType::globalSsh);

    EXPECT_NE(ssInHeapInfo1.surfaceStateOffset, ssInHeapInfo2.surfaceStateOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocatingMoreSsThenNewHeapAllocationCreated) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    size_t ssSize = 0x40;
    auto ssCount = bindlessHeapHelper->globalSsh->getAvailableSpace() / ssSize;
    auto graphicsAllocations = std::make_unique<MockGraphicsAllocation[]>(ssCount);
    auto ssAllocationBefore = bindlessHeapHelper->globalSsh->getGraphicsAllocation();
    for (uint32_t i = 0; i < ssCount; i++) {
        bindlessHeapHelper->allocateSSInHeap(ssSize, &graphicsAllocations[i], BindlessHeapsHelper::BindlesHeapType::globalSsh);
    }
    MockGraphicsAllocation alloc;
    bindlessHeapHelper->allocateSSInHeap(ssSize, &alloc, BindlessHeapsHelper::BindlesHeapType::globalSsh);

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

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInSpecialHeapThenFirstSlotIsAtOffsetZero) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::specialSsh);

    EXPECT_EQ(0u, ssInHeapInfo.surfaceStateOffset);
    EXPECT_EQ(ssInHeapInfo.heapAllocation->getGpuAddress(), ssInHeapInfo.heapAllocation->getGpuBaseAddress());
    EXPECT_EQ(bindlessHeapHelper->getGlobalHeapsBase(), ssInHeapInfo.heapAllocation->getGpuBaseAddress());
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInGlobalHeapThenOffsetLessThanHeapSize) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    EXPECT_LE(0u, ssInHeapInfo.surfaceStateOffset);
    EXPECT_GT(MemoryConstants::max32BitAddress, ssInHeapInfo.surfaceStateOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInGlobalDshThenOffsetLessThanHeapSize) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::globalDsh);
    EXPECT_LE(0u, ssInHeapInfo.surfaceStateOffset);
    EXPECT_GT(MemoryConstants::max32BitAddress, ssInHeapInfo.surfaceStateOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenFreeGraphicsMemoryIsCalledThenSSinHeapInfoShouldBePlacedInReuseVector) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(1);
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    MockBindlesHeapsHelper *bindlessHeapHelperPtr = bindlessHeapHelper.get();
    memManager->mockExecutionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->bindlessHeapsHelper.reset(bindlessHeapHelper.release());
    MockGraphicsAllocation *alloc = new MockGraphicsAllocation;
    memManager->allocateBindlessSlot(alloc);

    auto ssInHeapInfo = alloc->getBindlessInfo();

    memManager->freeGraphicsMemory(alloc);

    auto freePoolIndex = bindlessHeapHelperPtr->releasePoolIndex;

    EXPECT_EQ(bindlessHeapHelperPtr->surfaceStateInHeapVectorReuse[freePoolIndex][0].size(), 1u);
    auto ssInHeapInfoFromReuseVector = bindlessHeapHelperPtr->surfaceStateInHeapVectorReuse[freePoolIndex][0].front();
    EXPECT_EQ(ssInHeapInfoFromReuseVector.surfaceStateOffset, ssInHeapInfo.surfaceStateOffset);
    EXPECT_EQ(ssInHeapInfoFromReuseVector.ssPtr, ssInHeapInfo.ssPtr);

    MockGraphicsAllocation *alloc2 = new MockGraphicsAllocation;
    alloc2->allocationType = AllocationType::image;
    memManager->allocateBindlessSlot(alloc2);

    auto ssInHeapInfo2 = alloc2->getBindlessInfo();

    memManager->freeGraphicsMemory(alloc2);
    EXPECT_EQ(bindlessHeapHelperPtr->surfaceStateInHeapVectorReuse[freePoolIndex][1].size(), 1u);
    ssInHeapInfoFromReuseVector = bindlessHeapHelperPtr->surfaceStateInHeapVectorReuse[freePoolIndex][1].front();
    EXPECT_EQ(ssInHeapInfoFromReuseVector.surfaceStateOffset, ssInHeapInfo2.surfaceStateOffset);
    EXPECT_EQ(ssInHeapInfoFromReuseVector.ssPtr, ssInHeapInfo2.ssPtr);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocatingBindlessSlotTwiceThenNewSlotIsNotAllocatedAndTrueIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(1);
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    memManager->mockExecutionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->bindlessHeapsHelper.reset(bindlessHeapHelper.release());
    MockGraphicsAllocation *alloc = new MockGraphicsAllocation;
    EXPECT_TRUE(memManager->allocateBindlessSlot(alloc));
    auto ssInHeapInfo = alloc->getBindlessInfo();

    EXPECT_TRUE(memManager->allocateBindlessSlot(alloc));
    auto ssInHeapInfo2 = alloc->getBindlessInfo();

    EXPECT_EQ(ssInHeapInfo.ssPtr, ssInHeapInfo2.ssPtr);

    memManager->freeGraphicsMemory(alloc);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperPreviousAllocationThenItShouldNotBeReusedIfThresholdNotReached) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(1);
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    MockBindlesHeapsHelper *bindlessHeapHelperPtr = bindlessHeapHelper.get();
    MockGraphicsAllocation *alloc = new MockGraphicsAllocation;
    memManager->mockExecutionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->bindlessHeapsHelper.reset(bindlessHeapHelper.release());
    memManager->allocateBindlessSlot(alloc);

    auto ssInHeapInfo = alloc->getBindlessInfo();
    memManager->freeGraphicsMemory(alloc);

    auto freePoolIndex = bindlessHeapHelperPtr->releasePoolIndex;

    EXPECT_EQ(bindlessHeapHelperPtr->surfaceStateInHeapVectorReuse[freePoolIndex][0].size(), 1u);
    MockGraphicsAllocation *alloc2 = new MockGraphicsAllocation;

    memManager->allocateBindlessSlot(alloc2);
    auto newSSinHeapInfo = alloc2->getBindlessInfo();

    EXPECT_NE(bindlessHeapHelperPtr->surfaceStateInHeapVectorReuse[freePoolIndex][0].size(), 0u);
    EXPECT_NE(ssInHeapInfo.surfaceStateOffset, newSSinHeapInfo.surfaceStateOffset);
    EXPECT_NE(ssInHeapInfo.ssPtr, newSSinHeapInfo.ssPtr);
    memManager->freeGraphicsMemory(alloc2);
}

TEST_F(BindlessHeapsHelperTests, givenDeviceWhenBindlessHeapHelperInitializedThenCorrectDeviceBitFieldIsUsed) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(1);
    DeviceBitfield deviceBitfield = 7;
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, deviceBitfield);
    EXPECT_EQ(reinterpret_cast<MockMemoryManager *>(getMemoryManager())->recentlyPassedDeviceBitfield, deviceBitfield);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenCreatedThenAllocateAndReleasePoolIndicesAreInitializedToZero) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);

    EXPECT_FALSE(bindlessHeapHelper->allocateFromReusePool);
    EXPECT_EQ(0u, bindlessHeapHelper->allocatePoolIndex);
    EXPECT_EQ(0u, bindlessHeapHelper->releasePoolIndex);

    EXPECT_EQ(0u, bindlessHeapHelper->stateCacheDirtyForContext.to_ulong());
}

TEST_F(BindlessHeapsHelperTests, givenFreeSlotsExceedingThresholdInResuePoolWhenNewSlotsAllocatedThenSlotsAreAllocatedFromReusePool) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    bindlessHeapHelper->reuseSlotCountThreshold = 4;

    size_t size = bindlessHeapHelper->surfaceStateSize;

    SurfaceStateInHeapInfo ssInHeapInfos[5];

    ssInHeapInfos[0] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[1] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    // allocate double size for image
    ssInHeapInfos[2] = bindlessHeapHelper->allocateSSInHeap(2 * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[3] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[4] = bindlessHeapHelper->allocateSSInHeap(2 * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

    EXPECT_FALSE(bindlessHeapHelper->allocateFromReusePool);

    for (int i = 0; i < 5; i++) {
        bindlessHeapHelper->releaseSSToReusePool(ssInHeapInfos[i]);
        ssInHeapInfos[i] = {0};
    }

    ssInHeapInfos[4] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    EXPECT_TRUE(bindlessHeapHelper->allocateFromReusePool);

    EXPECT_EQ(0u, bindlessHeapHelper->allocatePoolIndex);
    EXPECT_EQ(1u, bindlessHeapHelper->releasePoolIndex);

    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), bindlessHeapHelper->stateCacheDirtyForContext.to_ullong());
}

TEST_F(BindlessHeapsHelperTests, givenReusePoolExhaustedWhenNewSlotsAllocatedThenSlotsAreNotResuedAndStateCacheDirtyFlagsAreNotSet) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    bindlessHeapHelper->reuseSlotCountThreshold = 4;

    size_t size = bindlessHeapHelper->surfaceStateSize;

    SurfaceStateInHeapInfo ssInHeapInfos[5];

    ssInHeapInfos[0] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[1] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    // allocate double size for image
    ssInHeapInfos[2] = bindlessHeapHelper->allocateSSInHeap(2 * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[3] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[4] = bindlessHeapHelper->allocateSSInHeap(2 * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

    EXPECT_FALSE(bindlessHeapHelper->allocateFromReusePool);

    for (int i = 0; i < 5; i++) {
        bindlessHeapHelper->releaseSSToReusePool(ssInHeapInfos[i]);
        ssInHeapInfos[i] = {0};
    }
    EXPECT_FALSE(bindlessHeapHelper->allocateFromReusePool);
    EXPECT_EQ(0u, bindlessHeapHelper->allocatePoolIndex);
    EXPECT_EQ(0u, bindlessHeapHelper->releasePoolIndex);

    for (int i = 0; i < 3; i++) {
        ssInHeapInfos[i] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    }

    EXPECT_EQ(0u, bindlessHeapHelper->allocatePoolIndex);
    EXPECT_EQ(1u, bindlessHeapHelper->releasePoolIndex);

    auto allocatePoolIndex = bindlessHeapHelper->allocatePoolIndex;
    auto releasePoolIndex = bindlessHeapHelper->releasePoolIndex;

    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][0].size(), 0u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[releasePoolIndex][1].size(), 2u);

    bindlessHeapHelper->stateCacheDirtyForContext.reset();

    ssInHeapInfos[3] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    EXPECT_NE(0u, ssInHeapInfos[3].surfaceStateOffset);
    EXPECT_NE(nullptr, ssInHeapInfos[3].ssPtr);

    EXPECT_FALSE(bindlessHeapHelper->allocateFromReusePool);
    EXPECT_EQ(0u, bindlessHeapHelper->stateCacheDirtyForContext.to_ullong());
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][0].size(), 0u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][1].size(), 0u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[releasePoolIndex][0].size(), 0u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[releasePoolIndex][1].size(), 2u);
}

TEST_F(BindlessHeapsHelperTests, givenReleasedSlotsToSecondPoolWhenThresholdReachedThenPoolsAreSwitchedAndSlotsAllocatedFromReusePool) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    bindlessHeapHelper->reuseSlotCountThreshold = 4;

    size_t size = bindlessHeapHelper->surfaceStateSize;

    SurfaceStateInHeapInfo ssInHeapInfos[5];

    ssInHeapInfos[0] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[1] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    // allocate double size for image
    ssInHeapInfos[2] = bindlessHeapHelper->allocateSSInHeap(2 * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[3] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[4] = bindlessHeapHelper->allocateSSInHeap(2 * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

    for (int i = 0; i < 5; i++) {
        bindlessHeapHelper->releaseSSToReusePool(ssInHeapInfos[i]);
        ssInHeapInfos[i] = {0};
    }

    for (int i = 0; i < 3; i++) {
        ssInHeapInfos[i] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    }

    EXPECT_EQ(0u, bindlessHeapHelper->allocatePoolIndex);
    EXPECT_EQ(1u, bindlessHeapHelper->releasePoolIndex);

    auto allocatePoolIndex = bindlessHeapHelper->allocatePoolIndex;
    auto releasePoolIndex = bindlessHeapHelper->releasePoolIndex;

    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][0].size(), 0u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[releasePoolIndex][1].size(), 2u);

    for (int i = 0; i < 3; i++) {
        bindlessHeapHelper->releaseSSToReusePool(ssInHeapInfos[i]);
        ssInHeapInfos[i] = {0};
    }

    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[releasePoolIndex][0].size(), 3u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[releasePoolIndex][1].size(), 2u);

    ssInHeapInfos[3] = bindlessHeapHelper->allocateSSInHeap(2 * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

    EXPECT_EQ(1u, bindlessHeapHelper->allocatePoolIndex);
    EXPECT_EQ(0u, bindlessHeapHelper->releasePoolIndex);

    allocatePoolIndex = bindlessHeapHelper->allocatePoolIndex;
    releasePoolIndex = bindlessHeapHelper->releasePoolIndex;

    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][1].size(), 1u);

    ssInHeapInfos[4] = bindlessHeapHelper->allocateSSInHeap(2 * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][1].size(), 0u);
    EXPECT_FALSE(bindlessHeapHelper->allocateFromReusePool);

    bindlessHeapHelper->releaseSSToReusePool(ssInHeapInfos[3]);
    bindlessHeapHelper->releaseSSToReusePool(ssInHeapInfos[4]);

    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][0].size(), 0u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][1].size(), 0u);

    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[releasePoolIndex][0].size(), 3u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[releasePoolIndex][1].size(), 2u);

    bindlessHeapHelper->stateCacheDirtyForContext.reset();

    ssInHeapInfos[0] = bindlessHeapHelper->allocateSSInHeap(2 * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

    EXPECT_EQ(0u, bindlessHeapHelper->allocatePoolIndex);
    EXPECT_EQ(1u, bindlessHeapHelper->releasePoolIndex);
    EXPECT_EQ(std::numeric_limits<uint64_t>::max(), bindlessHeapHelper->stateCacheDirtyForContext.to_ullong());
    EXPECT_TRUE(bindlessHeapHelper->allocateFromReusePool);

    allocatePoolIndex = bindlessHeapHelper->allocatePoolIndex;
    releasePoolIndex = bindlessHeapHelper->releasePoolIndex;

    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][0].size(), 3u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][1].size(), 1u);

    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[releasePoolIndex][0].size(), 0u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[releasePoolIndex][1].size(), 0u);

    for (int i = 0; i < 8; i++) {
        bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    }
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][0].size(), 0u);
    EXPECT_FALSE(bindlessHeapHelper->allocateFromReusePool);
}

TEST_F(BindlessHeapsHelperTests, givenFreeSlotsInReusePoolForONeSizeWhenAllocatingDifferentSizeThenNewSlotFromHeapIsAllocated) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);
    bindlessHeapHelper->reuseSlotCountThreshold = 4;

    size_t size = bindlessHeapHelper->surfaceStateSize;

    SurfaceStateInHeapInfo ssInHeapInfos[5];

    ssInHeapInfos[0] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[1] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[2] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[3] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[4] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

    for (int i = 0; i < 5; i++) {
        bindlessHeapHelper->releaseSSToReusePool(ssInHeapInfos[i]);
        ssInHeapInfos[i] = {0};
    }

    ssInHeapInfos[0] = bindlessHeapHelper->allocateSSInHeap(2 * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

    EXPECT_EQ(0u, bindlessHeapHelper->allocatePoolIndex);
    EXPECT_EQ(1u, bindlessHeapHelper->releasePoolIndex);

    EXPECT_NE(0u, ssInHeapInfos[0].surfaceStateOffset);
    EXPECT_NE(nullptr, ssInHeapInfos[0].ssPtr);

    auto allocatePoolIndex = bindlessHeapHelper->allocatePoolIndex;
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][0].size(), 5u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][1].size(), 0u);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHelperWhenGettingAndClearingDirstyStateForContextThenCorrectFlagIsReturnedAdCleard) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getMemoryManager(), false, rootDeviceIndex, devBitfield);

    EXPECT_FALSE(bindlessHeapHelper->getStateDirtyForContext(1));
    bindlessHeapHelper->stateCacheDirtyForContext.set(3);
    EXPECT_TRUE(bindlessHeapHelper->getStateDirtyForContext(3));

    bindlessHeapHelper->clearStateDirtyForContext(3);
    EXPECT_FALSE(bindlessHeapHelper->getStateDirtyForContext(3));
}