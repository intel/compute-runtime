/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/fixtures/memory_allocator_multi_device_fixture.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using MemoryManagerMultiDeviceTest = MemoryAllocatorMultiDeviceFixture<10>;

TEST_P(MemoryManagerMultiDeviceTest, givenRootDeviceIndexSpecifiedWhenAllocateGraphicsMemoryIsCalledThenGraphicsAllocationHasTheSameRootDeviceIndex) {
    std::vector<AllocationType> allocationTypes{AllocationType::BUFFER,
                                                AllocationType::KERNEL_ISA};
    for (auto allocationType : allocationTypes) {
        for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < getNumRootDevices(); ++rootDeviceIndex) {
            AllocationProperties properties{rootDeviceIndex, true, MemoryConstants::pageSize, allocationType, false, false, mockDeviceBitfield};

            auto gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);
            ASSERT_NE(gfxAllocation, nullptr);
            EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
            memoryManager->freeGraphicsMemory(gfxAllocation);

            gfxAllocation = memoryManager->allocateGraphicsMemoryWithProperties(properties, (void *)0x1234);
            ASSERT_NE(gfxAllocation, nullptr);
            EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
            memoryManager->freeGraphicsMemory(gfxAllocation);

            gfxAllocation = memoryManager->allocateGraphicsMemoryInPreferredPool(properties, nullptr);
            ASSERT_NE(gfxAllocation, nullptr);
            EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
            memoryManager->freeGraphicsMemory(gfxAllocation);

            gfxAllocation = memoryManager->allocateGraphicsMemoryInPreferredPool(properties, (void *)0x1234);
            ASSERT_NE(gfxAllocation, nullptr);
            EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
            memoryManager->freeGraphicsMemory(gfxAllocation);

            gfxAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(static_cast<osHandle>(0u), properties, false, false, true);
            ASSERT_NE(gfxAllocation, nullptr);
            EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
            memoryManager->freeGraphicsMemory(gfxAllocation);

            gfxAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(static_cast<osHandle>(0u), properties, true, false, true);
            ASSERT_NE(gfxAllocation, nullptr);
            EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
            memoryManager->freeGraphicsMemory(gfxAllocation);
        }
    }
}

INSTANTIATE_TEST_CASE_P(MemoryManagerType, MemoryManagerMultiDeviceTest, ::testing::Bool());

TEST_P(MemoryManagerMultiDeviceTest, givenRootDeviceIndexSpecifiedWhenAllocateGraphicsMemoryIsCalledThenGraphicsAllocationHasProperGpuAddress) {
    RootDeviceIndicesContainer rootDeviceIndices;

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < getNumRootDevices(); ++rootDeviceIndex) {
        rootDeviceIndices.push_back(rootDeviceIndex);
    }
    rootDeviceIndices.remove_duplicates();

    auto maxRootDeviceIndex = *std::max_element(rootDeviceIndices.begin(), rootDeviceIndices.end(), std::less<uint32_t const>());
    auto tagsMultiAllocation = new MultiGraphicsAllocation(maxRootDeviceIndex);

    AllocationProperties unifiedMemoryProperties{rootDeviceIndices.at(0), MemoryConstants::pageSize, AllocationType::TAG_BUFFER, systemMemoryBitfield};

    memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, unifiedMemoryProperties, *tagsMultiAllocation);
    EXPECT_NE(nullptr, tagsMultiAllocation);

    auto graphicsAllocation0 = tagsMultiAllocation->getGraphicsAllocation(0);

    for (auto graphicsAllocation : tagsMultiAllocation->getGraphicsAllocations()) {
        EXPECT_EQ(graphicsAllocation->getUnderlyingBuffer(), graphicsAllocation0->getUnderlyingBuffer());
    }

    for (auto graphicsAllocation : tagsMultiAllocation->getGraphicsAllocations()) {
        memoryManager->freeGraphicsMemory(graphicsAllocation);
    }
    delete tagsMultiAllocation;
}

TEST_P(MemoryManagerMultiDeviceTest, givenRootDeviceIndexSpecifiedWhenAllocateGraphicsMemoryIsCalledThenAllocationPropertiesUsmFlagIsSetAccordingToAddressRange) {
    RootDeviceIndicesContainer rootDeviceIndices;

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < getNumRootDevices(); ++rootDeviceIndex) {
        rootDeviceIndices.push_back(rootDeviceIndex);
    }
    rootDeviceIndices.remove_duplicates();

    auto maxRootDeviceIndex = *std::max_element(rootDeviceIndices.begin(), rootDeviceIndices.end(), std::less<uint32_t const>());
    auto tagsMultiAllocation = new MultiGraphicsAllocation(maxRootDeviceIndex);

    AllocationProperties unifiedMemoryProperties{rootDeviceIndices.at(0), MemoryConstants::pageSize, AllocationType::TAG_BUFFER, systemMemoryBitfield};

    memoryManager->createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, unifiedMemoryProperties, *tagsMultiAllocation);
    EXPECT_NE(nullptr, tagsMultiAllocation);

    for (auto rootDeviceIndex : rootDeviceIndices) {
        if (memoryManager->isLimitedRange(rootDeviceIndex)) {
            EXPECT_EQ(unifiedMemoryProperties.flags.isUSMHostAllocation, false);
        } else {
            EXPECT_EQ(unifiedMemoryProperties.flags.isUSMHostAllocation, true);
        }
    }

    for (auto graphicsAllocation : tagsMultiAllocation->getGraphicsAllocations()) {
        memoryManager->freeGraphicsMemory(graphicsAllocation);
    }
    delete tagsMultiAllocation;
}
