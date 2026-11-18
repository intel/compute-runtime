/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace NEO;

namespace {
class PhysicalReleaseTrackingMemoryManager : public OsAgnosticMemoryManager {
  public:
    using OsAgnosticMemoryManager::mapPhysicalDeviceMemoryToVirtualMemory;
    using OsAgnosticMemoryManager::OsAgnosticMemoryManager;
    using OsAgnosticMemoryManager::unMapPhysicalDeviceMemoryFromVirtualMemory;

    void removeAllocationFromDownloadAllocationsInCsr(GraphicsAllocation *alloc) override {
        if (alloc == trackedPhysicalAllocation.load()) {
            physicalReleaseCount++;
        } else if (physicalReleaseCount.load() != 0u) {
            mappingsTornDownAfterPhysicalRelease++;
        }
        OsAgnosticMemoryManager::removeAllocationFromDownloadAllocationsInCsr(alloc);
    }

    std::atomic<GraphicsAllocation *> trackedPhysicalAllocation{nullptr};
    std::atomic<uint32_t> physicalReleaseCount{0u};
    std::atomic<uint32_t> mappingsTornDownAfterPhysicalRelease{0u};
};

void releasePhysicalMemoryConcurrentlyWithUnmappingItsMappings(uint32_t mappingCount) {
    constexpr uint32_t iterationCount = 100u;
    constexpr size_t mappingSize = MemoryConstants::pageSize;
    constexpr uint64_t firstGpuRange = 0x100000;

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    executionEnvironment.initGmm();
    MemoryManagerCreate<PhysicalReleaseTrackingMemoryManager> memoryManager(false, true, executionEnvironment);

    AllocationProperties properties(mockRootDeviceIndex, 1, AllocationType::buffer, mockDeviceBitfield);
    properties.flags.isHostInaccessibleAllocation = true;
    MemoryFlags flags = {};

    for (uint32_t iteration = 0; iteration < iterationCount; iteration++) {
        auto physicalAllocation = memoryManager.allocatePhysicalGraphicsMemory(properties);
        ASSERT_NE(nullptr, physicalAllocation);
        memoryManager.trackedPhysicalAllocation = physicalAllocation;
        memoryManager.physicalReleaseCount = 0u;
        memoryManager.mappingsTornDownAfterPhysicalRelease = 0u;

        std::vector<MultiGraphicsAllocation> mappings;
        mappings.reserve(mappingCount);
        for (uint32_t mappingIndex = 0; mappingIndex < mappingCount; mappingIndex++) {
            mappings.emplace_back(mockRootDeviceIndex);
            EXPECT_TRUE(memoryManager.mapPhysicalDeviceMemoryToVirtualMemory(mappings[mappingIndex], physicalAllocation, firstGpuRange * (mappingIndex + 1), mappingSize, &flags, 0u));
        }

        std::atomic_bool started = false;
        std::vector<std::thread> threads;
        for (uint32_t mappingIndex = 0; mappingIndex < mappingCount; mappingIndex++) {
            threads.emplace_back([&, mappingIndex]() {
                while (!started.load()) {
                    std::this_thread::yield();
                }
                auto mappedAllocation = mappings[mappingIndex].getGraphicsAllocation(mockRootDeviceIndex);
                EXPECT_TRUE(memoryManager.unMapPhysicalDeviceMemoryFromVirtualMemory(mappings[mappingIndex], mappedAllocation, firstGpuRange * (mappingIndex + 1), mappingSize, nullptr, 0u));
            });
        }
        threads.emplace_back([&]() {
            while (!started.load()) {
                std::this_thread::yield();
            }
            memoryManager.freeGraphicsMemory(physicalAllocation);
        });

        started = true;
        for (auto &thread : threads) {
            thread.join();
        }

        EXPECT_EQ(1u, memoryManager.physicalReleaseCount.load());
        EXPECT_EQ(0u, memoryManager.mappingsTornDownAfterPhysicalRelease.load());
    }
}
} // namespace

TEST(PhysicalMemoryMappingReleaseMtTest, givenLastMappingUnmappedConcurrentlyWithPhysicalReleaseThenPhysicalAllocationOutlivesMappingAndIsReleasedOnce) {
    releasePhysicalMemoryConcurrentlyWithUnmappingItsMappings(1u);
}

TEST(PhysicalMemoryMappingReleaseMtTest, givenMappingsUnmappedConcurrentlyWithEachOtherAndPhysicalReleaseThenPhysicalAllocationOutlivesMappingsAndIsReleasedOnce) {
    releasePhysicalMemoryConcurrentlyWithUnmappingItsMappings(3u);
}
