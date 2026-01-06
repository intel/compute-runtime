/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/utilities/heap_allocator.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_gfx_partition.h"
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
        rootDevice = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get(), rootDeviceIndex));
        memManager = static_cast<MockMemoryManager *>(rootDevice->getMemoryManager());
        rootDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
    }
    void tearDown() {}

    Device *getDevice() {
        return rootDevice.get();
    }

    MemoryManager *getMemoryManager() {
        return memManager;
    }

    DebugManagerStateRestore dbgRestorer;
    std::unique_ptr<MockDevice> rootDevice;
    MockMemoryManager *memManager = nullptr;
    uint32_t rootDeviceIndex = 0;
    DeviceBitfield devBitfield = 1;
    int genericSubDevices = 1;
};

using BindlessHeapsHelperTests = Test<BindlessHeapsHelperFixture>;

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenItsCreatedThenSpecialSshAllocatedAtHeapBegining) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    auto specialSshAllocation = bindlessHeapHelper->specialSsh->getGraphicsAllocation();
    EXPECT_EQ(specialSshAllocation->getGpuAddress(), specialSshAllocation->getGpuBaseAddress());
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapThenHeapUsedSpaceGrow) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    auto usedBefore = bindlessHeapHelper->globalSsh->getUsed();

    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    auto usedAfter = bindlessHeapHelper->globalSsh->getUsed();
    EXPECT_GT(usedAfter, usedBefore);
}

TEST_F(BindlessHeapsHelperTests, givenNoMemoryAvailableWhenAllocateSsInHeapThenNullptrIsReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

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
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    auto bindlessHeapHelperPtr = bindlessHeapHelper.get();
    memManager->peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->bindlessHeapsHelper.reset(bindlessHeapHelper.release());
    MockGraphicsAllocation alloc;

    bindlessHeapHelperPtr->globalSsh->getSpace(bindlessHeapHelperPtr->globalSsh->getAvailableSpace());
    memManager->failAllocateSystemMemory = true;
    memManager->failAllocate32Bit = true;

    EXPECT_FALSE(memManager->allocateBindlessSlot(&alloc));
    EXPECT_EQ(nullptr, alloc.getBindlessInfo().ssPtr);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapThenMemoryAtReturnedOffsetIsCorrect) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::globalSsh);

    auto allocInHeapPtr = bindlessHeapHelper->globalSsh->getGraphicsAllocation()->getUnderlyingBuffer();
    EXPECT_EQ(ssInHeapInfo.ssPtr, allocInHeapPtr);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapForImageThenThreeBindlessSlotsAreAllocated) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    auto surfaceStateSize = bindlessHeapHelper->surfaceStateSize;
    memManager->peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->bindlessHeapsHelper.reset(bindlessHeapHelper.release());

    MockGraphicsAllocation alloc;
    alloc.allocationType = AllocationType::image;
    EXPECT_TRUE(getMemoryManager()->allocateBindlessSlot(&alloc));
    auto ssInHeapInfo1 = alloc.getBindlessInfo();

    EXPECT_EQ(surfaceStateSize * NEO::BindlessImageSlot::max, ssInHeapInfo1.ssSize);

    MockGraphicsAllocation alloc2;
    alloc2.allocationType = AllocationType::sharedImage;
    EXPECT_TRUE(getMemoryManager()->allocateBindlessSlot(&alloc2));
    auto ssInHeapInfo2 = alloc2.getBindlessInfo();

    EXPECT_EQ(surfaceStateSize * NEO::BindlessImageSlot::max, ssInHeapInfo2.ssSize);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapTwiceForTheSameAllocationThenTheSameOffsetReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

    MockGraphicsAllocation alloc;

    EXPECT_TRUE(getMemoryManager()->allocateBindlessSlot(&alloc));
    auto ssInHeapInfo1 = alloc.getBindlessInfo();

    EXPECT_TRUE(getMemoryManager()->allocateBindlessSlot(&alloc));
    auto ssInHeapInfo2 = alloc.getBindlessInfo();
    EXPECT_EQ(ssInHeapInfo1.surfaceStateOffset, ssInHeapInfo2.surfaceStateOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInHeapTwiceForDifferentAllocationsThenDifferentOffsetsReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

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
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
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
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    for (auto allocation : bindlessHeapHelper->ssHeapsAllocations) {
        EXPECT_EQ(allocation->getGpuBaseAddress(), bindlessHeapHelper->getGlobalHeapsBase());
    }
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenGetDefaultBorderColorOffsetCalledThenCorrectOffsetReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    auto expectedOffset = bindlessHeapHelper->borderColorStates->getGpuAddress() - bindlessHeapHelper->borderColorStates->getGpuBaseAddress();
    EXPECT_EQ(bindlessHeapHelper->getDefaultBorderColorOffset(), expectedOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenGetAlphaBorderColorOffsetCalledThenCorrectOffsetReturned) {
    auto borderColorSize = 0x40;
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    auto expectedOffset = bindlessHeapHelper->borderColorStates->getGpuAddress() - bindlessHeapHelper->borderColorStates->getGpuBaseAddress() + borderColorSize;
    EXPECT_EQ(bindlessHeapHelper->getAlphaBorderColorOffset(), expectedOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInSpecialHeapThenFirstSlotIsAtOffsetZero) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    if (bindlessHeapHelper->heaplessEnabled) {
        GTEST_SKIP();
    }
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::specialSsh);

    EXPECT_EQ(0u, ssInHeapInfo.surfaceStateOffset);
    EXPECT_EQ(ssInHeapInfo.heapAllocation->getGpuAddress(), ssInHeapInfo.heapAllocation->getGpuBaseAddress());
    EXPECT_EQ(bindlessHeapHelper->getGlobalHeapsBase(), ssInHeapInfo.heapAllocation->getGpuBaseAddress());
}

TEST_F(BindlessHeapsHelperTests, givenHeaplessAndBindlessHeapHelperWhenAllocateSsInSpecialHeapThenFirstSlotIsAtOffsetOfHeapBaseAddress) {

    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    if (!bindlessHeapHelper->heaplessEnabled) {
        GTEST_SKIP();
    }
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::specialSsh);

    EXPECT_EQ(ssInHeapInfo.heapAllocation->getGpuBaseAddress(), ssInHeapInfo.surfaceStateOffset);
    EXPECT_EQ(ssInHeapInfo.heapAllocation->getGpuAddress(), ssInHeapInfo.heapAllocation->getGpuBaseAddress());
    EXPECT_EQ(bindlessHeapHelper->getGlobalHeapsBase(), ssInHeapInfo.heapAllocation->getGpuBaseAddress());
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInGlobalHeapThenOffsetLessThanHeapSize) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    EXPECT_LE(0u, ssInHeapInfo.surfaceStateOffset);
    if (bindlessHeapHelper->heaplessEnabled) {
        EXPECT_GT(MemoryConstants::max32BitAddress, ssInHeapInfo.surfaceStateOffset - ssInHeapInfo.heapAllocation->getGpuBaseAddress());
    } else {
        EXPECT_GT(MemoryConstants::max32BitAddress, ssInHeapInfo.surfaceStateOffset);
    }
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenAllocateSsInGlobalDshThenOffsetLessThanHeapSize) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::globalDsh);
    EXPECT_LE(0u, ssInHeapInfo.surfaceStateOffset);
    if (bindlessHeapHelper->heaplessEnabled) {
        EXPECT_GT(MemoryConstants::max32BitAddress, ssInHeapInfo.surfaceStateOffset - ssInHeapInfo.heapAllocation->getGpuBaseAddress());
    } else {
        EXPECT_GT(MemoryConstants::max32BitAddress, ssInHeapInfo.surfaceStateOffset);
    }
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenFreeGraphicsMemoryIsCalledThenSSinHeapInfoShouldBePlacedInReuseVector) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(1);
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    MockBindlesHeapsHelper *bindlessHeapHelperPtr = bindlessHeapHelper.get();
    memManager->peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->bindlessHeapsHelper.reset(bindlessHeapHelper.release());
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
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    memManager->peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->bindlessHeapsHelper.reset(bindlessHeapHelper.release());
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
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    MockBindlesHeapsHelper *bindlessHeapHelperPtr = bindlessHeapHelper.get();
    MockGraphicsAllocation *alloc = new MockGraphicsAllocation;
    memManager->peekExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->bindlessHeapsHelper.reset(bindlessHeapHelper.release());
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
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    EXPECT_EQ(reinterpret_cast<MockMemoryManager *>(getMemoryManager())->recentlyPassedDeviceBitfield, getDevice()->getDeviceBitfield());
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenCreatedThenAllocateAndReleasePoolIndicesAreInitializedToZero) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

    EXPECT_FALSE(bindlessHeapHelper->allocateFromReusePool);
    EXPECT_EQ(0u, bindlessHeapHelper->allocatePoolIndex);
    EXPECT_EQ(0u, bindlessHeapHelper->releasePoolIndex);

    EXPECT_EQ(0u, bindlessHeapHelper->stateCacheDirtyForContext.to_ulong());
}

TEST_F(BindlessHeapsHelperTests, givenFreeSlotsExceedingThresholdInResuePoolWhenNewSlotsAllocatedThenSlotsAreAllocatedFromReusePool) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    bindlessHeapHelper->reuseSlotCountThreshold = 4;

    size_t size = bindlessHeapHelper->surfaceStateSize;

    SurfaceStateInHeapInfo ssInHeapInfos[5];

    ssInHeapInfos[0] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[1] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    // allocate triple size for image
    ssInHeapInfos[2] = bindlessHeapHelper->allocateSSInHeap(NEO::BindlessImageSlot::max * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[3] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[4] = bindlessHeapHelper->allocateSSInHeap(NEO::BindlessImageSlot::max * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

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
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    bindlessHeapHelper->reuseSlotCountThreshold = 4;

    size_t size = bindlessHeapHelper->surfaceStateSize;

    SurfaceStateInHeapInfo ssInHeapInfos[5];

    ssInHeapInfos[0] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[1] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    // allocate triple size for image
    ssInHeapInfos[2] = bindlessHeapHelper->allocateSSInHeap(NEO::BindlessImageSlot::max * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[3] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[4] = bindlessHeapHelper->allocateSSInHeap(NEO::BindlessImageSlot::max * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

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
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    bindlessHeapHelper->reuseSlotCountThreshold = 4;

    size_t size = bindlessHeapHelper->surfaceStateSize;

    SurfaceStateInHeapInfo ssInHeapInfos[5];

    ssInHeapInfos[0] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[1] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    // allocate triple size for image
    ssInHeapInfos[2] = bindlessHeapHelper->allocateSSInHeap(NEO::BindlessImageSlot::max * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[3] = bindlessHeapHelper->allocateSSInHeap(size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    ssInHeapInfos[4] = bindlessHeapHelper->allocateSSInHeap(NEO::BindlessImageSlot::max * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

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

    ssInHeapInfos[3] = bindlessHeapHelper->allocateSSInHeap(NEO::BindlessImageSlot::max * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

    EXPECT_EQ(1u, bindlessHeapHelper->allocatePoolIndex);
    EXPECT_EQ(0u, bindlessHeapHelper->releasePoolIndex);

    allocatePoolIndex = bindlessHeapHelper->allocatePoolIndex;
    releasePoolIndex = bindlessHeapHelper->releasePoolIndex;

    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][1].size(), 1u);

    ssInHeapInfos[4] = bindlessHeapHelper->allocateSSInHeap(NEO::BindlessImageSlot::max * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][1].size(), 0u);
    EXPECT_FALSE(bindlessHeapHelper->allocateFromReusePool);

    bindlessHeapHelper->releaseSSToReusePool(ssInHeapInfos[3]);
    bindlessHeapHelper->releaseSSToReusePool(ssInHeapInfos[4]);

    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][0].size(), 0u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][1].size(), 0u);

    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[releasePoolIndex][0].size(), 3u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[releasePoolIndex][1].size(), 2u);

    bindlessHeapHelper->stateCacheDirtyForContext.reset();

    ssInHeapInfos[0] = bindlessHeapHelper->allocateSSInHeap(NEO::BindlessImageSlot::max * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

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
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
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

    ssInHeapInfos[0] = bindlessHeapHelper->allocateSSInHeap(NEO::BindlessImageSlot::max * size, nullptr, BindlessHeapsHelper::BindlesHeapType::globalSsh);

    EXPECT_EQ(0u, bindlessHeapHelper->allocatePoolIndex);
    EXPECT_EQ(1u, bindlessHeapHelper->releasePoolIndex);

    EXPECT_NE(0u, ssInHeapInfos[0].surfaceStateOffset);
    EXPECT_NE(nullptr, ssInHeapInfos[0].ssPtr);

    auto allocatePoolIndex = bindlessHeapHelper->allocatePoolIndex;
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][0].size(), 5u);
    EXPECT_EQ(bindlessHeapHelper->surfaceStateInHeapVectorReuse[allocatePoolIndex][1].size(), 0u);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHelperWhenGettingAndClearingDirstyStateForContextThenCorrectFlagIsReturnedAdCleard) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

    EXPECT_FALSE(bindlessHeapHelper->getStateDirtyForContext(1));
    bindlessHeapHelper->stateCacheDirtyForContext.set(3);
    EXPECT_TRUE(bindlessHeapHelper->getStateDirtyForContext(3));

    bindlessHeapHelper->clearStateDirtyForContext(3);
    EXPECT_FALSE(bindlessHeapHelper->getStateDirtyForContext(3));
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenItsCreatedThenSshAllocationsAreResident) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);
    MemoryOperationsHandler *memoryOperationsIface = getDevice()->getRootDeviceEnvironmentRef().memoryOperationsInterface.get();

    for (int heapType = 0; heapType < BindlessHeapsHelper::BindlesHeapType::numHeapTypes; heapType++) {
        auto *allocation = bindlessHeapHelper->getHeap(static_cast<BindlessHeapsHelper::BindlesHeapType>(heapType))->getGraphicsAllocation();
        EXPECT_EQ(memoryOperationsIface->isResident(getDevice(), *allocation), MemoryOperationsStatus::success);
    }
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenDriverModelWDDMThenReservedMemoryModeIsAvailable) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

    getDevice()->getRootDeviceEnvironmentRef().osInterface.reset(new NEO::OSInterface());

    getDevice()->getRootDeviceEnvironmentRef().osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());
    EXPECT_TRUE(bindlessHeapHelper->isReservedMemoryModeAvailable());

    getDevice()->getRootDeviceEnvironmentRef().osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelDRM>());
    EXPECT_FALSE(bindlessHeapHelper->isReservedMemoryModeAvailable());
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenSuccessfullyReservingMemoryRangeThenRangeIsReservedAndStoredAndFreed) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

    size_t reservationSize = 1 * MemoryConstants::gigaByte;
    size_t alignment = MemoryConstants::pageSize64k;
    HeapIndex heapIndex = HeapIndex::heapStandard;

    auto reservedRange = bindlessHeapHelper->reserveMemoryRange(reservationSize, alignment, heapIndex);
    ASSERT_TRUE(reservedRange.has_value());

    EXPECT_EQ(reservationSize, reservedRange->size);

    EXPECT_EQ(1u, bindlessHeapHelper->reservedRanges.size());
    EXPECT_EQ(bindlessHeapHelper->reservedRanges[0].address, reservedRange->address);
    EXPECT_EQ(bindlessHeapHelper->reservedRanges[0].size, reservedRange->size);

    EXPECT_EQ(1u, memManager->reserveGpuAddressOnHeapCalled);

    bindlessHeapHelper.reset();

    EXPECT_EQ(1u, memManager->freeGpuAddressCalled);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenUnsuccessfullyReservingMemoryRangeThenNoValueIsReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

    size_t reservationSize = 1 * MemoryConstants::gigaByte;
    size_t alignment = MemoryConstants::pageSize64k;
    HeapIndex heapIndex = HeapIndex::heapStandard;

    memManager->failReserveGpuAddressOnHeap = true;

    auto reservedRange = bindlessHeapHelper->reserveMemoryRange(reservationSize, alignment, heapIndex);
    EXPECT_FALSE(reservedRange.has_value());

    EXPECT_EQ(0u, bindlessHeapHelper->reservedRanges.size());
    EXPECT_EQ(1u, memManager->reserveGpuAddressOnHeapCalled);

    bindlessHeapHelper.reset();

    EXPECT_EQ(0u, memManager->freeGpuAddressCalled);
}

TEST_F(BindlessHeapsHelperTests, givenLocalMemorySupportWhenReservingMemoryForSpecialSshThenCorrectHeapIsUsed) {
    auto gfxPartition = std::make_unique<MockGfxPartition>();
    gfxPartition->callHeapAllocate = false;
    memManager->gfxPartitions[0] = std::move(gfxPartition);

    std::map<bool, HeapIndex> localMemSupportedToExpectedHeapIndexMap = {
        {false, HeapIndex::heapExternalFrontWindow},
        {true, HeapIndex::heapExternalDeviceFrontWindow}};

    size_t currentIter = 0u;

    for (auto &[localMemSupported, expectedHeapIndex] : localMemSupportedToExpectedHeapIndexMap) {
        auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

        size_t reservationSize = MemoryConstants::pageSize64k;
        size_t alignment = MemoryConstants::pageSize64k;

        memManager->localMemorySupported = std::vector<bool>{localMemSupported};

        auto specialSshReservationSuccessful = bindlessHeapHelper->tryReservingMemoryForSpecialSsh(reservationSize, alignment);
        EXPECT_TRUE(specialSshReservationSuccessful);

        auto &reserveGpuAddressOnHeapParamsPassed = memManager->reserveGpuAddressOnHeapParamsPassed;
        ASSERT_GE(reserveGpuAddressOnHeapParamsPassed.size(), currentIter + 1);
        EXPECT_EQ(expectedHeapIndex, reserveGpuAddressOnHeapParamsPassed[currentIter].heap);

        EXPECT_EQ(1u, bindlessHeapHelper->reservedRanges.size());
        EXPECT_EQ(currentIter + 1u, memManager->reserveGpuAddressOnHeapCalled);

        bindlessHeapHelper.reset();

        EXPECT_EQ(currentIter + 1u, memManager->freeGpuAddressCalled);
        currentIter++;
    }
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenSpecialSshReservationFailsThenNoRangeIsReserved) {
    memManager->failReserveGpuAddressOnHeap = true;

    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

    size_t reservationSize = MemoryConstants::pageSize64k;
    size_t alignment = MemoryConstants::pageSize64k;

    auto specialSshReservationSuccessful = bindlessHeapHelper->tryReservingMemoryForSpecialSsh(reservationSize, alignment);
    EXPECT_FALSE(specialSshReservationSuccessful);

    EXPECT_EQ(0u, bindlessHeapHelper->reservedRanges.size());
    EXPECT_EQ(1u, memManager->reserveGpuAddressOnHeapCalled);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenReservedMemoryAlreadyInitializedThenEarlyReturnTrue) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

    bindlessHeapHelper->reservedMemoryInitialized = true;
    memManager->reserveGpuAddressOnHeapCalled = 0u;

    EXPECT_TRUE(bindlessHeapHelper->initializeReservedMemory());
    EXPECT_EQ(0u, memManager->reserveGpuAddressOnHeapCalled);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenMemoryReservationFailsDuringInitializationThenInitializationReturnsFalse) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

    memManager->reserveGpuAddressOnHeapCalled = 0u;
    memManager->failReserveGpuAddressOnHeap = true;

    EXPECT_FALSE(bindlessHeapHelper->initializeReservedMemory());
    EXPECT_EQ(1u, memManager->reserveGpuAddressOnHeapCalled);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHeapHelperWhenSuccessfullyInitializingReservedMemoryThenHeapsAndAllocatorsAreConfiguredCorrectly) {
    constexpr uint64_t fullHeapSize = 4 * MemoryConstants::gigaByte;

    if (fullHeapSize > std::numeric_limits<size_t>::max()) {
        GTEST_SKIP();
    }

    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

    memManager->reserveGpuAddressOnHeapCalled = 0u;
    memManager->customHeapAllocators.clear();

    // Override gfxPartition to ensure heapStandard has sufficient free/available space for this test.
    auto mockGfxPartition = std::make_unique<MockGfxPartition>();
    mockGfxPartition->initHeap(HeapIndex::heapStandard, maxNBitValue(56) + 1, MemoryConstants::teraByte, MemoryConstants::pageSize64k);
    memManager->gfxPartitions[0] = std::move(mockGfxPartition);

    EXPECT_TRUE(bindlessHeapHelper->initializeReservedMemory());

    EXPECT_EQ(1u, memManager->reserveGpuAddressOnHeapCalled);
    EXPECT_TRUE(bindlessHeapHelper->reservedMemoryInitialized);

    auto &reserveGpuAddressOnHeapParamsPassed = memManager->reserveGpuAddressOnHeapParamsPassed;
    ASSERT_EQ(1u, reserveGpuAddressOnHeapParamsPassed.size());

    EXPECT_EQ(HeapIndex::heapStandard, reserveGpuAddressOnHeapParamsPassed[0].heap);
    EXPECT_EQ(4 * MemoryConstants::gigaByte, reserveGpuAddressOnHeapParamsPassed[0].size);
    EXPECT_EQ(MemoryConstants::pageSize64k, reserveGpuAddressOnHeapParamsPassed[0].alignment);

    EXPECT_EQ(rootDevice->getRootDeviceEnvironmentRef().getGmmHelper()->decanonize(memManager->reserveGpuAddressOnHeapResult.address), bindlessHeapHelper->reservedRangeBase);

    ASSERT_EQ(1u, bindlessHeapHelper->reservedRanges.size());
    EXPECT_EQ(memManager->reserveGpuAddressOnHeapResult.address, bindlessHeapHelper->reservedRanges[0].address);
    EXPECT_EQ(memManager->reserveGpuAddressOnHeapResult.size, bindlessHeapHelper->reservedRanges[0].size);

    constexpr auto expectedFrontWindowSize = GfxPartition::externalFrontWindowPoolSize;

    {
        // heapFrontWindow
        EXPECT_EQ(bindlessHeapHelper->heapFrontWindow->getBaseAddress(), bindlessHeapHelper->reservedRangeBase);
        auto frontWindowSize = bindlessHeapHelper->heapFrontWindow->getLeftSize() + bindlessHeapHelper->heapFrontWindow->getUsedSize();
        EXPECT_EQ(expectedFrontWindowSize, frontWindowSize);
    }

    {
        // heapRegular
        EXPECT_EQ(bindlessHeapHelper->heapRegular->getBaseAddress(), bindlessHeapHelper->heapFrontWindow->getBaseAddress() + expectedFrontWindowSize);
        auto expectedRegularSize = 4 * MemoryConstants::gigaByte - expectedFrontWindowSize;
        auto heapRegularSize = bindlessHeapHelper->heapRegular->getLeftSize() + bindlessHeapHelper->heapRegular->getUsedSize();
        EXPECT_EQ(expectedRegularSize, heapRegularSize);
    }

    EXPECT_EQ(2u, memManager->customHeapAllocators.size());

    {
        // heapFrontWindow
        ASSERT_TRUE(memManager->getCustomHeapAllocatorConfig(AllocationType::linearStream, true).has_value());
        EXPECT_EQ(bindlessHeapHelper->heapFrontWindow.get(), memManager->getCustomHeapAllocatorConfig(AllocationType::linearStream, true)->get().allocator);
        EXPECT_EQ(bindlessHeapHelper->reservedRangeBase, memManager->getCustomHeapAllocatorConfig(AllocationType::linearStream, true)->get().gpuVaBase);
    }

    {
        // heapRegular
        ASSERT_TRUE(memManager->getCustomHeapAllocatorConfig(AllocationType::linearStream, false).has_value());
        EXPECT_EQ(bindlessHeapHelper->heapRegular.get(), memManager->getCustomHeapAllocatorConfig(AllocationType::linearStream, false)->get().allocator);
        EXPECT_EQ(bindlessHeapHelper->reservedRangeBase, memManager->getCustomHeapAllocatorConfig(AllocationType::linearStream, false)->get().gpuVaBase);
    }

    bindlessHeapHelper.reset();

    EXPECT_EQ(1u, memManager->freeGpuAddressCalled); // 1 * 4GB reserved range
    EXPECT_EQ(0u, memManager->customHeapAllocators.size());
}

TEST_F(BindlessHeapsHelperTests, givenReservedMemoryModeAvailableWhenSpecialSshReservationInFrontWindowFailsThenReservedMemoryModeIsUsed) {
    auto gfxPartition = std::make_unique<MockGfxPartition>();
    gfxPartition->callHeapAllocate = false;
    memManager->gfxPartitions[0] = std::move(gfxPartition);

    getDevice()->getRootDeviceEnvironmentRef().osInterface.reset(new NEO::OSInterface());
    getDevice()->getRootDeviceEnvironmentRef().osInterface->setDriverModel(std::make_unique<NEO::MockDriverModelWDDM>());

    memManager->reserveGpuAddressOnHeapFailOnCalls.push_back(0u); // Fail reserving memory for special ssh

    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(getDevice(), false);

    EXPECT_TRUE(bindlessHeapHelper->reservedMemoryInitialized);
    EXPECT_TRUE(bindlessHeapHelper->useReservedMemory);
}