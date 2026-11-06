/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/unified_memory/unified_memory.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/context/context.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

#include <atomic>
#include <thread>
#include <vector>

namespace L0 {
namespace ult {

class ContextWhiteboxForIpcMtTest : public ::L0::Context {
  public:
    ContextWhiteboxForIpcMtTest(L0::DriverHandle *driverHandle) : ::L0::Context(driverHandle) {}
    using ::L0::Context::ipcRangeTransportMutex;
    using ::L0::Context::ipcRangeTransports;
    using ::L0::Context::ipcRangeTransportsPresent;
    using ::L0::Context::releaseIpcRangeChunkHandles;
    using ::L0::Context::setIPCHandleData;
};

class HandleCloseSignalingMemoryManager : public NEO::MockMemoryManager {
  public:
    void closeInternalHandle(uint64_t &handle, uint32_t handleId, NEO::GraphicsAllocation *graphicsAllocation) override {
        if (graphicsAllocation == this->releasedAllocation) {
            this->releasedHandleClosed.store(true, std::memory_order_release);
            while (!this->rangeHandlePutStarted.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        }
    }

    NEO::GraphicsAllocation *releasedAllocation = nullptr;
    std::atomic<bool> releasedHandleClosed{false};
    std::atomic<bool> rangeHandlePutStarted{false};
};

using ContextIpcSocketMtTest = Test<DeviceFixture>;

TEST_F(ContextIpcSocketMtTest, givenConcurrentSetIpcHandleDataWhenCalledFromMultipleThreadsThenNoRace) {
    constexpr int numThreads = 4;
    constexpr int handlesPerThread = 100;
    std::atomic<bool> go{false};
    std::vector<std::thread> threads;

    ContextWhiteboxForIpcMtTest contextWhitebox(driverHandle.get());
    contextWhitebox.settings.handleType = L0::IpcHandleType::ntHandle;
    std::vector<NEO::MockGraphicsAllocation> allocs(numThreads);

    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t] {
            while (!go.load(std::memory_order_acquire)) {
            }
            for (int i = 0; i < handlesPerThread; ++i) {
                uint64_t h = static_cast<uint64_t>(t * handlesPerThread + i + 1);
                L0::IpcOpaqueMemoryData opaqueIpcData = {};
                contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(
                    {.alloc = &allocs[t], .handle = h, .ptrAddress = h * 0x1000u, .memoryType = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory)},
                    opaqueIpcData);
            }
        });
    }

    go.store(true, std::memory_order_release);
    for (auto &th : threads) {
        th.join();
    }

    EXPECT_EQ(static_cast<size_t>(numThreads * handlesPerThread),
              driverHandle->getIPCHandleMap().size());

    auto &map = driverHandle->getIPCHandleMap();
    for (auto &[k, v] : map) {
        delete v;
    }
    map.clear();
}

TEST_F(ContextIpcSocketMtTest, givenHandleReusedByConcurrentExportWhenReleasingIpcHandlesOfAllocationAndPuttingRangeHandleThenReusedHandleTrackingIsKept) {
    constexpr uint64_t reusedHandle = 101u;

    HandleCloseSignalingMemoryManager signalingMemoryManager;
    auto previousMemoryManager = driverHandle->getMemoryManager();
    driverHandle->setMemoryManager(&signalingMemoryManager);

    ContextWhiteboxForIpcMtTest contextWhitebox(driverHandle.get());
    contextWhitebox.settings.handleType = L0::IpcHandleType::ntHandle;

    NEO::MockGraphicsAllocation releasedAlloc;
    NEO::MockGraphicsAllocation reusingAlloc;
    signalingMemoryManager.releasedAllocation = &releasedAlloc;

    L0::IpcOpaqueMemoryData opaqueIpcData = {};
    contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(
        {.alloc = &releasedAlloc, .handle = reusedHandle, .ptrAddress = 0x1000u, .memoryType = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory)},
        opaqueIpcData);
    contextWhitebox.ipcRangeTransports.push_back({.handleKeys = {reusedHandle}});
    contextWhitebox.ipcRangeTransportsPresent.store(true);

    std::atomic<bool> reusedHandleExported{false};
    std::thread releasing([&] {
        contextWhitebox.releaseIpcHandlesOfAllocation(&releasedAlloc);
    });
    std::thread exporting([&] {
        while (!signalingMemoryManager.releasedHandleClosed.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        L0::IpcOpaqueMemoryData reusingIpcData = {};
        contextWhitebox.setIPCHandleData<L0::IpcOpaqueMemoryData>(
            {.alloc = &reusingAlloc, .handle = reusedHandle, .ptrAddress = 0x2000u, .memoryType = static_cast<uint8_t>(InternalMemoryType::deviceUnifiedMemory)},
            reusingIpcData);
        reusedHandleExported.store(true, std::memory_order_release);
    });
    std::thread putting([&] {
        while (!signalingMemoryManager.releasedHandleClosed.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        auto &transportMutex = contextWhitebox.ipcRangeTransportMutex;
        const bool transportLockedBeforeRelease = transportMutex.try_lock();
        signalingMemoryManager.rangeHandlePutStarted.store(true, std::memory_order_release);
        if (!transportLockedBeforeRelease) {
            transportMutex.lock();
        }
        while (!reusedHandleExported.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
        contextWhitebox.releaseIpcRangeChunkHandles(contextWhitebox.ipcRangeTransports.front().handleKeys);
        transportMutex.unlock();
    });
    releasing.join();
    exporting.join();
    putting.join();
    driverHandle->setMemoryManager(previousMemoryManager);

    auto &ipcMap = driverHandle->getIPCHandleMap();
    auto reusedTracking = ipcMap.find(reusedHandle);
    ASSERT_NE(ipcMap.end(), reusedTracking);
    EXPECT_EQ(&reusingAlloc, reusedTracking->second->alloc);
    EXPECT_EQ(1u, ipcMap.size());

    for (auto &[handle, tracking] : ipcMap) {
        delete tracking;
    }
    ipcMap.clear();
}

} // namespace ult
} // namespace L0
