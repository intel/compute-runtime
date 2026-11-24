/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/fixtures/memory_allocator_multi_device_fixture.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include <algorithm>
#include <map>

using namespace NEO;

using MemoryManagerMultiDeviceTest = MemoryAllocatorMultiDeviceFixture<10>;

TEST_P(MemoryManagerMultiDeviceTest, givenRootDeviceIndexSpecifiedWhenAllocateGraphicsMemoryIsCalledThenGraphicsAllocationHasTheSameRootDeviceIndex) {
    std::vector<AllocationType> allocationTypes{AllocationType::buffer,
                                                AllocationType::kernelIsa};
    for (auto allocationType : allocationTypes) {
        for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < getNumRootDevices(); ++rootDeviceIndex) {
            AllocationProperties properties{rootDeviceIndex, true, MemoryConstants::pageSize, allocationType, false, false, mockDeviceBitfield};
            MemoryManager::OsHandleData osHandleData{static_cast<uint64_t>(0ull)};

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

            gfxAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
            ASSERT_NE(gfxAllocation, nullptr);
            EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
            memoryManager->freeGraphicsMemory(gfxAllocation);

            gfxAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, true, false, true, nullptr);
            ASSERT_NE(gfxAllocation, nullptr);
            EXPECT_EQ(rootDeviceIndex, gfxAllocation->getRootDeviceIndex());
            memoryManager->freeGraphicsMemory(gfxAllocation);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(MemoryManagerType, MemoryManagerMultiDeviceTest, ::testing::Bool());

TEST_P(MemoryManagerMultiDeviceTest, givenRootDeviceIndexSpecifiedWhenAllocateGraphicsMemoryIsCalledThenGraphicsAllocationHasProperGpuAddress) {
    RootDeviceIndicesContainer rootDeviceIndices;

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < getNumRootDevices(); ++rootDeviceIndex) {
        rootDeviceIndices.pushUnique(rootDeviceIndex);
    }

    auto maxRootDeviceIndex = *std::max_element(rootDeviceIndices.begin(), rootDeviceIndices.end(), std::less<uint32_t const>());
    auto tagsMultiAllocation = new MultiGraphicsAllocation(maxRootDeviceIndex);

    AllocationProperties unifiedMemoryProperties{rootDeviceIndices.at(0), MemoryConstants::pageSize, AllocationType::tagBuffer, systemMemoryBitfield};

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

TEST(MemoryManagerMultiRootDeviceTest, givenRootDeviceIndexSpecifiedWhenAllocateGraphicsMemoryIsCalledThenAllocationPropertiesUsmFlagIsSetAccordingToAddressRange) {
    constexpr uint32_t numRootDevices = 4u;
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, numRootDevices);
    RootDeviceIndicesContainer rootDeviceIndices;

    for (uint32_t rootDeviceIndex = 0; rootDeviceIndex < numRootDevices; ++rootDeviceIndex) {
        rootDeviceIndices.pushUnique(rootDeviceIndex);
    }

    auto maxRootDeviceIndex = *std::max_element(rootDeviceIndices.begin(), rootDeviceIndices.end(), std::less<uint32_t const>());
    MultiGraphicsAllocation tagsMultiAllocation(maxRootDeviceIndex);

    MockMemoryManager mockMemoryManager(executionEnvironment);
    std::map<uint32_t, bool> usmHostAllocationFlagPerRootDevice;
    mockMemoryManager.validateAllocateProperties = [&usmHostAllocationFlagPerRootDevice](const AllocationProperties &properties) {
        usmHostAllocationFlagPerRootDevice[properties.rootDeviceIndex] = properties.flags.isUSMHostAllocation;
    };

    AllocationProperties unifiedMemoryProperties{rootDeviceIndices.at(0), MemoryConstants::pageSize, AllocationType::tagBuffer, systemMemoryBitfield};

    EXPECT_NE(nullptr, mockMemoryManager.createMultiGraphicsAllocationInSystemMemoryPool(rootDeviceIndices, unifiedMemoryProperties, tagsMultiAllocation));
    EXPECT_FALSE(unifiedMemoryProperties.flags.isUSMHostAllocation);

    EXPECT_EQ(rootDeviceIndices.size(), usmHostAllocationFlagPerRootDevice.size());
    for (auto rootDeviceIndex : rootDeviceIndices) {
        EXPECT_TRUE(usmHostAllocationFlagPerRootDevice[rootDeviceIndex]);
    }

    for (auto graphicsAllocation : tagsMultiAllocation.getGraphicsAllocations()) {
        mockMemoryManager.freeGraphicsMemory(graphicsAllocation);
    }
}
