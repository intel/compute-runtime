/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/test/unit_test/fixtures/front_window_fixture.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"
#include "shared/test/unit_test/mocks/mock_device.h"
#include "shared/test/unit_test/mocks/mock_graphics_allocation.h"
#include "shared/test/unit_test/mocks/ult_device_factory.h"

#include "test.h"

using namespace NEO;

class MockBindlesHeapsHelper : public BindlessHeapsHelper {
  public:
    using BaseClass = BindlessHeapsHelper;
    MockBindlesHeapsHelper(MemoryManager *memManager, bool isMultiOsContextCapable, const uint32_t rootDeviceIndex) : BaseClass(memManager, isMultiOsContextCapable, rootDeviceIndex) {
        globalSsh = surfaceStateHeaps[BindlesHeapType::GLOBAL_SSH].get();
        specialSsh = surfaceStateHeaps[BindlesHeapType::SPECIAL_SSH].get();
        scratchSsh = surfaceStateHeaps[BindlesHeapType::SPECIAL_SSH].get();
        globalDsh = surfaceStateHeaps[BindlesHeapType::SPECIAL_SSH].get();
    }
    using BindlesHeapType = BindlessHeapsHelper::BindlesHeapType;
    using BaseClass::borderColorStates;
    using BaseClass::growHeap;
    using BaseClass::isMultiOsContextCapable;
    using BaseClass::memManager;
    using BaseClass::rootDeviceIndex;
    using BaseClass::ssHeapsAllocations;
    using BaseClass::surfaceStateInHeapAllocationMap;

    IndirectHeap *specialSsh;
    IndirectHeap *globalSsh;
    IndirectHeap *scratchSsh;
    IndirectHeap *globalDsh;
};

TEST(BindlessHeapsHelper, givenBindlessModeFlagEnabledWhenCreatingRootDevicesThenBindlesHeapHelperCreated) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    EXPECT_NE(deviceFactory->rootDevices[0]->getBindlessHeapsHelper(), nullptr);
}

TEST(BindlessHeapsHelper, givenBindlessModeFlagDisabledWhenCreatingRootDevicesThenBindlesHeapHelperCreated) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(0);
    std::unique_ptr<UltDeviceFactory> deviceFactory(new UltDeviceFactory(1, 1));
    EXPECT_EQ(deviceFactory->rootDevices[0]->getBindlessHeapsHelper(), nullptr);
}

using BindlessHeapsHelperTests = Test<MemManagerFixture>;

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenItsCreatedThenSpecialSshAllocatedAtHeapBegining) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    auto specialSshAllocation = bindlessHeapHelper->specialSsh->getGraphicsAllocation();
    EXPECT_EQ(specialSshAllocation->getGpuAddress(), specialSshAllocation->getGpuBaseAddress());
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocatSsInHeapThenHeapUsedSpaceGrow) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    auto usedBefore = bindlessHeapHelper->globalSsh->getUsed();

    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    auto usedAfter = bindlessHeapHelper->globalSsh->getUsed();
    EXPECT_GT(usedAfter, usedBefore);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocateSsInHeapThenMemoryAtReturnedOffsetIsCorrect) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());

    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);

    auto allocInHeapPtr = bindlessHeapHelper->globalSsh->getGraphicsAllocation()->getUnderlyingBuffer();
    EXPECT_EQ(ssInHeapInfo.ssPtr, allocInHeapPtr);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocateSsInHeapTwiceForTheSameAllocationThenTheSameOffsetReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());

    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo1 = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    auto ssInHeapInfo2 = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);

    EXPECT_EQ(ssInHeapInfo1.surfaceStateOffset, ssInHeapInfo2.surfaceStateOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocateSsInHeapTwiceForDifferentAllocationThenDifferentOffsetsReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());

    MockGraphicsAllocation alloc1;
    MockGraphicsAllocation alloc2;
    size_t size = 0x40;
    auto ss = std::make_unique<uint8_t[]>(size);
    memset(ss.get(), 35, size);
    auto ssInHeapInfo1 = bindlessHeapHelper->allocateSSInHeap(size, &alloc1, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    auto ssInHeapInfo2 = bindlessHeapHelper->allocateSSInHeap(size, &alloc2, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);

    EXPECT_NE(ssInHeapInfo1.surfaceStateOffset, ssInHeapInfo2.surfaceStateOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocateMoreSsThanNewHeapAllocationCreated) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
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

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenCreatedThenAllocationsHaveTheSameBaseAddress) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    for (auto allocation : bindlessHeapHelper->ssHeapsAllocations) {
        EXPECT_EQ(allocation->getGpuBaseAddress(), bindlessHeapHelper->getGlobalHeapsBase());
    }
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenGetDefaultBorderColorOffsetCalledThenCorrectOffsetReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    auto expectedOffset = bindlessHeapHelper->borderColorStates->getGpuAddress() - bindlessHeapHelper->borderColorStates->getGpuBaseAddress();
    EXPECT_EQ(bindlessHeapHelper->getDefaultBorderColorOffset(), expectedOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenGetAlphaBorderColorOffsetCalledThenCorrectOffsetReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    auto expectedOffset = bindlessHeapHelper->borderColorStates->getGpuAddress() - bindlessHeapHelper->borderColorStates->getGpuBaseAddress() + 4 * sizeof(float);
    EXPECT_EQ(bindlessHeapHelper->getAlphaBorderColorOffset(), expectedOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocatSsInSpecialHeapThenOffsetLessThanFrontWindowSize) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::SPECIAL_SSH);
    auto frontWindowSize = GfxPartition::externalFrontWindowPoolSize;
    EXPECT_LT(ssInHeapInfo.surfaceStateOffset, frontWindowSize);
}
TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocatSsInGlobalHeapThenOffsetLessThanFrontWindowSize) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    auto frontWindowSize = GfxPartition::externalFrontWindowPoolSize;
    EXPECT_LT(ssInHeapInfo.surfaceStateOffset, frontWindowSize);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocatSsInScratchHeapThenOffsetLessThanFrontWindowSize) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::SCRATCH_SSH);
    auto frontWindowSize = GfxPartition::externalFrontWindowPoolSize;
    EXPECT_LT(ssInHeapInfo.surfaceStateOffset, frontWindowSize);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocatSsInGlobalDshThenOffsetGreaterOrEqualFrontWindowSize) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ssInHeapInfo = bindlessHeapHelper->allocateSSInHeap(size, &alloc, BindlessHeapsHelper::BindlesHeapType::GLOBAL_DSH);
    auto frontWindowSize = GfxPartition::externalFrontWindowPoolSize;
    EXPECT_GE(ssInHeapInfo.surfaceStateOffset, frontWindowSize);
}