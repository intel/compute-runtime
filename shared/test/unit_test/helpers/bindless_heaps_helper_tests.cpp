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
    MockBindlesHeapsHelper(MemoryManager *memManager, bool isMultiOsContextCapable, const uint32_t rootDeviceIndex) : BaseClass(memManager, isMultiOsContextCapable, rootDeviceIndex) {}
    using BaseClass::borderColorStates;
    using BaseClass::globalSsh;
    using BaseClass::growGlobalSSh;
    using BaseClass::isMultiOsContextCapable;
    using BaseClass::memManager;
    using BaseClass::rootDeviceIndex;
    using BaseClass::specialSsh;
    using BaseClass::ssHeapsAllocations;
    using BaseClass::surfaceStateInHeapAllocationMap;
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
    auto ss = std::make_unique<uint8_t[]>(size);
    memset(ss.get(), 35, size);
    bindlessHeapHelper->allocateSSInHeap(size, ss.get(), &alloc);
    auto usedAfter = bindlessHeapHelper->globalSsh->getUsed();
    EXPECT_GT(usedAfter, usedBefore);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocateSsInHeapThenMemoryAtReturnedOffsetIsCorrect) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());

    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ss = std::make_unique<uint8_t[]>(size);
    memset(ss.get(), 35, size);
    bindlessHeapHelper->allocateSSInHeap(size, ss.get(), &alloc);

    auto allocInHeapPtr = bindlessHeapHelper->globalSsh->getGraphicsAllocation()->getUnderlyingBuffer();
    EXPECT_EQ(memcmp(ss.get(), allocInHeapPtr, size), 0);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocateSsInHeapTwiceForTheSameAllocationThenTheSameOffsetReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());

    MockGraphicsAllocation alloc;
    size_t size = 0x40;
    auto ss = std::make_unique<uint8_t[]>(size);
    memset(ss.get(), 35, size);
    auto ssInHeapInfo1 = bindlessHeapHelper->allocateSSInHeap(size, ss.get(), &alloc);
    auto ssInHeapInfo2 = bindlessHeapHelper->allocateSSInHeap(size, ss.get(), &alloc);

    EXPECT_EQ(ssInHeapInfo1->surfaceStateOffset, ssInHeapInfo2->surfaceStateOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocateSsInHeapTwiceForDifferentAllocationThenDifferentOffsetsReturned) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());

    MockGraphicsAllocation alloc1;
    MockGraphicsAllocation alloc2;
    size_t size = 0x40;
    auto ss = std::make_unique<uint8_t[]>(size);
    memset(ss.get(), 35, size);
    auto ssInHeapInfo1 = bindlessHeapHelper->allocateSSInHeap(size, ss.get(), &alloc1);
    auto ssInHeapInfo2 = bindlessHeapHelper->allocateSSInHeap(size, ss.get(), &alloc2);

    EXPECT_NE(ssInHeapInfo1->surfaceStateOffset, ssInHeapInfo2->surfaceStateOffset);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenAllocateMoreSsThanNewHeapAllocationCreated) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    size_t ssSize = 0x40;
    auto ssCount = bindlessHeapHelper->globalSsh->getAvailableSpace() / ssSize;
    auto graphicsAllocations = std::make_unique<MockGraphicsAllocation[]>(ssCount);
    auto ssAllocationBefore = bindlessHeapHelper->globalSsh->getGraphicsAllocation();
    auto ss = std::make_unique<uint8_t[]>(ssSize);
    memset(ss.get(), 35, ssSize);
    for (uint32_t i = 0; i < ssCount; i++) {
        bindlessHeapHelper->allocateSSInHeap(ssSize, ss.get(), &graphicsAllocations[i]);
    }
    MockGraphicsAllocation alloc;
    bindlessHeapHelper->allocateSSInHeap(ssSize, ss.get(), &alloc);

    auto ssAllocationAfter = bindlessHeapHelper->globalSsh->getGraphicsAllocation();

    EXPECT_NE(ssAllocationBefore, ssAllocationAfter);
}

TEST_F(BindlessHeapsHelperTests, givenBindlessHepaHelperWhenCreatedThenAllocationsHaveTheSameBaseAddress) {
    auto bindlessHeapHelper = std::make_unique<MockBindlesHeapsHelper>(pDevice->getMemoryManager(), pDevice->getNumAvailableDevices() > 1, pDevice->getRootDeviceIndex());
    for (auto allocation : bindlessHeapHelper->ssHeapsAllocations) {
        EXPECT_EQ(allocation->getGpuBaseAddress(), bindlessHeapHelper->getGlobalSshBase());
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