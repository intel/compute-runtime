/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

using namespace NEO;

namespace {

class MtProbeMockMemoryManager : public MockMemoryManager {
  public:
    using MockMemoryManager::freeGraphicsMemory;
    using MockMemoryManager::MockMemoryManager;

    GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
        if (properties.flags.shareable == 0u) {
            return MockMemoryManager::allocateGraphicsMemoryWithProperties(properties);
        }
        probeAllocateCalls++;
        auto allocation = new MockGraphicsAllocation(properties.rootDeviceIndex, nullptr, MemoryConstants::pageSize);
        allocation->internalHandle = 0x42;
        registerProbeAllocation(allocation);
        return allocation;
    }

    GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
        importCalls++;
        auto allocation = new MockGraphicsAllocation(properties.rootDeviceIndex, nullptr, MemoryConstants::pageSize);
        registerProbeAllocation(allocation);
        return allocation;
    }

    void freeGraphicsMemory(GraphicsAllocation *gfxAllocation, bool isImportedAllocation) override {
        if (unregisterProbeAllocation(gfxAllocation)) {
            probeFreeCalls++;
            delete gfxAllocation;
            return;
        }
        MockMemoryManager::freeGraphicsMemory(gfxAllocation, isImportedAllocation);
    }

    void closeInternalHandle(uint64_t &handle, uint32_t handleId, GraphicsAllocation *graphicsAllocation) override {
        closeCalls++;
    }

    std::atomic<uint32_t> probeAllocateCalls{0u};
    std::atomic<uint32_t> importCalls{0u};
    std::atomic<uint32_t> probeFreeCalls{0u};
    std::atomic<uint32_t> closeCalls{0u};

  protected:
    void registerProbeAllocation(GraphicsAllocation *allocation) {
        std::lock_guard<std::mutex> lock(probeAllocationsMutex);
        probeAllocations.push_back(allocation);
    }

    bool unregisterProbeAllocation(GraphicsAllocation *allocation) {
        std::lock_guard<std::mutex> lock(probeAllocationsMutex);
        auto it = std::find(probeAllocations.begin(), probeAllocations.end(), allocation);
        if (it == probeAllocations.end()) {
            return false;
        }
        probeAllocations.erase(it);
        return true;
    }

    std::mutex probeAllocationsMutex;
    std::vector<GraphicsAllocation *> probeAllocations;
};

} // namespace

TEST(DeviceCanAccessPeerDrmMtTests, givenTwoDevicesWhenCanAccessPeerRunsRealProbeFromMultiThreadsInBothWaysThenProbeRunsOnceAndProbeResourcesReleasedOnce) {
    constexpr uint32_t numRootDevices = 2u;
    auto executionEnvironment = new MockExecutionEnvironment(defaultHwInfo.get(), false, numRootDevices);
    executionEnvironment->memoryManager.reset(new MtProbeMockMemoryManager(*executionEnvironment));
    auto memoryManager = static_cast<MtProbeMockMemoryManager *>(executionEnvironment->memoryManager.get());
    for (uint32_t i = 0; i < numRootDevices; ++i) {
        executionEnvironment->rootDeviceEnvironments[i]->osInterface.reset(new OSInterface());
        auto drm = new DrmMock{*executionEnvironment->rootDeviceEnvironments[i]};
        executionEnvironment->rootDeviceEnvironments[i]->osInterface->setDriverModel(std::unique_ptr<DriverModel>{drm});
    }

    auto device0 = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 0));
    auto device1 = std::unique_ptr<MockDevice>(MockDevice::createWithExecutionEnvironment<MockDevice>(defaultHwInfo.get(), executionEnvironment, 1));

    std::atomic<uint32_t> queryCalled = 0;

    auto queryPeerAccess = [&queryCalled](Device &device, Device &peerDevice, GraphicsAllocation **probeAllocPtr, uint64_t *handle) -> bool {
        queryCalled++;
        return device.Device::queryPeerAccess(peerDevice, probeAllocPtr, handle);
    };
    device0->queryPeerAccessFunc = queryPeerAccess;
    device1->queryPeerAccessFunc = queryPeerAccess;

    std::atomic_bool started = false;
    constexpr int numThreads = 8;
    constexpr int iterationCount = 20;
    std::vector<std::thread> threads;

    auto threadBody = [&](int threadId) {
        while (!started.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        auto device = device0.get();
        auto peerDevice = device1.get();

        if (threadId & 1) {
            device = device1.get();
            peerDevice = device0.get();
        }
        for (auto i = 0; i < iterationCount; i++) {
            bool canAccess = device->canAccessPeer(peerDevice);
            EXPECT_TRUE(canAccess);
        }
    };

    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(threadBody, i);
    }

    started = true;

    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(1u, queryCalled.load());
    EXPECT_EQ(1u, memoryManager->probeAllocateCalls.load());
    EXPECT_EQ(1u, memoryManager->importCalls.load());
    EXPECT_EQ(1u, memoryManager->closeCalls.load());
    EXPECT_EQ(2u, memoryManager->probeFreeCalls.load());
}
