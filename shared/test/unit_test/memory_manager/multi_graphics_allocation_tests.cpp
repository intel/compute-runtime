/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

using namespace NEO;

struct MockMultiGraphicsAllocation : public MultiGraphicsAllocation {
    using MultiGraphicsAllocation::graphicsAllocations;
    using MultiGraphicsAllocation::lastUsedRootDeviceIndex;
    using MultiGraphicsAllocation::MultiGraphicsAllocation;
};

TEST(MultiGraphicsAllocationTest, whenCreatingMultiGraphicsAllocationThenTheAllocationIsObtainableAsADefault) {
    GraphicsAllocation graphicsAllocation(1, // rootDeviceIndex
                                          GraphicsAllocation::AllocationType::BUFFER,
                                          nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(1);
    EXPECT_EQ(2u, multiGraphicsAllocation.graphicsAllocations.size());

    multiGraphicsAllocation.addAllocation(&graphicsAllocation);

    EXPECT_EQ(&graphicsAllocation, multiGraphicsAllocation.getDefaultGraphicsAllocation());

    EXPECT_EQ(&graphicsAllocation, multiGraphicsAllocation.getGraphicsAllocation(graphicsAllocation.getRootDeviceIndex()));
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(0));
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenAddingMultipleGraphicsAllocationsThenTheyAreObtainableByRootDeviceIndex) {
    GraphicsAllocation graphicsAllocation0(0, // rootDeviceIndex
                                           GraphicsAllocation::AllocationType::BUFFER,
                                           nullptr, 0, 0, MemoryPool::System4KBPages, 0);
    GraphicsAllocation graphicsAllocation1(1, // rootDeviceIndex
                                           GraphicsAllocation::AllocationType::BUFFER,
                                           nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(1);

    EXPECT_EQ(2u, multiGraphicsAllocation.graphicsAllocations.size());
    multiGraphicsAllocation.addAllocation(&graphicsAllocation0);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation1);

    EXPECT_EQ(&graphicsAllocation0, multiGraphicsAllocation.getGraphicsAllocation(graphicsAllocation0.getRootDeviceIndex()));
    EXPECT_EQ(&graphicsAllocation1, multiGraphicsAllocation.getGraphicsAllocation(graphicsAllocation1.getRootDeviceIndex()));
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenGettingAllocationTypeThenReturnAllocationTypeFromDefaultAllocation) {
    auto expectedAllocationType = GraphicsAllocation::AllocationType::BUFFER;
    GraphicsAllocation graphicsAllocation(1, // rootDeviceIndex
                                          expectedAllocationType,
                                          nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(1);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation);

    EXPECT_EQ(multiGraphicsAllocation.getDefaultGraphicsAllocation()->getAllocationType(), multiGraphicsAllocation.getAllocationType());
    EXPECT_EQ(expectedAllocationType, multiGraphicsAllocation.getAllocationType());
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenGettingCoherencyStatusThenReturnCoherencyStatusFromDefaultAllocation) {
    auto expectedAllocationType = GraphicsAllocation::AllocationType::BUFFER;
    GraphicsAllocation graphicsAllocation(1, // rootDeviceIndex
                                          expectedAllocationType,
                                          nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(1);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation);

    graphicsAllocation.setCoherent(true);
    EXPECT_TRUE(multiGraphicsAllocation.isCoherent());

    graphicsAllocation.setCoherent(false);
    EXPECT_FALSE(multiGraphicsAllocation.isCoherent());
}

TEST(MultiGraphicsAllocationTest, WhenCreatingMultiGraphicsAllocationWithoutGraphicsAllocationThenNoDefaultAllocationIsReturned) {
    MockMultiGraphicsAllocation multiGraphicsAllocation(1);
    EXPECT_EQ(nullptr, multiGraphicsAllocation.getDefaultGraphicsAllocation());
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenRemovingGraphicsAllocationThenTheAllocationIsNoLongerAvailable) {
    uint32_t rootDeviceIndex = 1u;
    GraphicsAllocation graphicsAllocation(rootDeviceIndex,
                                          GraphicsAllocation::AllocationType::BUFFER,
                                          nullptr, 0, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(rootDeviceIndex);
    EXPECT_EQ(2u, multiGraphicsAllocation.graphicsAllocations.size());

    multiGraphicsAllocation.addAllocation(&graphicsAllocation);

    EXPECT_EQ(&graphicsAllocation, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));

    multiGraphicsAllocation.removeAllocation(rootDeviceIndex);

    EXPECT_EQ(nullptr, multiGraphicsAllocation.getGraphicsAllocation(rootDeviceIndex));
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenEnsureMemoryOnDeviceIsCalledThenDataIsProperlyTransferred) {
    constexpr auto bufferSize = 4u;

    uint8_t hostBuffer[bufferSize] = {1u, 1u, 1u, 1u};
    uint8_t refBuffer[bufferSize] = {3u, 3u, 3u, 3u};

    GraphicsAllocation graphicsAllocation1(1u, GraphicsAllocation::AllocationType::BUFFER, hostBuffer, bufferSize, 0, MemoryPool::LocalMemory, 0);
    GraphicsAllocation graphicsAllocation2(2u, GraphicsAllocation::AllocationType::BUFFER, refBuffer, bufferSize, 0, MemoryPool::LocalMemory, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(2u);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation1);
    multiGraphicsAllocation.addAllocation(&graphicsAllocation2);

    MockExecutionEnvironment mockExecutionEnvironment(defaultHwInfo.get());
    MockMemoryManager mockMemoryManager(mockExecutionEnvironment);

    multiGraphicsAllocation.lastUsedRootDeviceIndex = 1u;
    multiGraphicsAllocation.ensureMemoryOnDevice(mockMemoryManager, 2u);

    auto underlyingBuffer1 = multiGraphicsAllocation.getGraphicsAllocation(1u)->getUnderlyingBuffer();
    auto ptrUnderlyingBuffer1 = static_cast<uint8_t *>(underlyingBuffer1);

    auto underlyingBuffer2 = multiGraphicsAllocation.getGraphicsAllocation(2u)->getUnderlyingBuffer();
    auto ptrUnderlyingBuffer2 = static_cast<uint8_t *>(underlyingBuffer2);

    for (auto i = 0u; i < bufferSize; i++) {
        EXPECT_EQ(ptrUnderlyingBuffer1[i], ptrUnderlyingBuffer2[i]);
    }
}

TEST(MultiGraphicsAllocationTest, givenMultiGraphicsAllocationWhenEnsureMemoryOnDeviceIsCalledThenLockAndUnlockAreProperlyCalled) {
    constexpr auto bufferSize = 4u;

    uint8_t hostBuffer[bufferSize] = {1u, 1u, 1u, 1u};
    uint8_t refBuffer[bufferSize] = {3u, 3u, 3u, 3u};

    MemoryAllocation allocation1(1u, GraphicsAllocation::AllocationType::BUFFER, hostBuffer, bufferSize, 0, MemoryPool::System4KBPages, 0);
    MemoryAllocation allocation2(2u, GraphicsAllocation::AllocationType::BUFFER, refBuffer, bufferSize, 0, MemoryPool::System4KBPages, 0);

    MockMultiGraphicsAllocation multiGraphicsAllocation(2u);
    multiGraphicsAllocation.addAllocation(&allocation1);
    multiGraphicsAllocation.addAllocation(&allocation2);

    MockExecutionEnvironment mockExecutionEnvironment(defaultHwInfo.get());
    MockMemoryManager mockMemoryManager(mockExecutionEnvironment);

    multiGraphicsAllocation.ensureMemoryOnDevice(mockMemoryManager, 1u);
    EXPECT_EQ(mockMemoryManager.lockResourceCalled, 0u);
    EXPECT_EQ(mockMemoryManager.unlockResourceCalled, 0u);

    multiGraphicsAllocation.ensureMemoryOnDevice(mockMemoryManager, 1u);
    EXPECT_EQ(mockMemoryManager.lockResourceCalled, 0u);
    EXPECT_EQ(mockMemoryManager.unlockResourceCalled, 0u);

    multiGraphicsAllocation.ensureMemoryOnDevice(mockMemoryManager, 2u);
    EXPECT_EQ(mockMemoryManager.lockResourceCalled, 0u);
    EXPECT_EQ(mockMemoryManager.unlockResourceCalled, 0u);

    multiGraphicsAllocation.ensureMemoryOnDevice(mockMemoryManager, 1u);
    EXPECT_EQ(mockMemoryManager.lockResourceCalled, 0u);
    EXPECT_EQ(mockMemoryManager.unlockResourceCalled, 0u);

    (&allocation1)->overrideMemoryPool(MemoryPool::LocalMemory);
    (&allocation2)->overrideMemoryPool(MemoryPool::LocalMemory);
    multiGraphicsAllocation.ensureMemoryOnDevice(mockMemoryManager, 2u);
    EXPECT_EQ(mockMemoryManager.lockResourceCalled, 2u);
    EXPECT_EQ(mockMemoryManager.unlockResourceCalled, 2u);
}
