/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_os_context_win.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/mock_sys_calls.h"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>
#include <vector>

using namespace NEO;

struct BlockingKmdWaitWddmMock : WddmMock {
    using WddmMock::WddmMock;

    bool waitForMonitoredFenceKmdWaitEvent(HANDLE, uint32_t) override {
        const auto callIndex = waitCalled.fetch_add(1);
        if (callIndex == 0) {
            firstWaitEntered = true;
            while (!finishFirstWait) {
                std::this_thread::yield();
            }
        }
        return true;
    }

    bool resetMonitoredFenceKmdWaitEvent(HANDLE) override {
        resetCalled++;
        return false;
    }

    std::atomic<uint32_t> waitCalled = 0;
    std::atomic<uint32_t> resetCalled = 0;
    std::atomic<bool> firstWaitEntered = false;
    std::atomic<bool> finishFirstWait = false;
};

TEST(WddmKmdWaitMTTest, givenKmdWaitInProgressWhenAnotherThreadWaitsForNewerFenceThenItFallsBackWithoutReusingTheEvent) {
    MockExecutionEnvironment executionEnvironment;
    executionEnvironment.initializeMemoryManager();
    auto *rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    BlockingKmdWaitWddmMock wddm(*rootDeviceEnvironment);

    auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular});
    MockOsContextWin osContext(wddm, 0u, 0u, engineDescriptor);
    uint64_t fenceValue = 0;
    D3DKMT_HANDLE fenceHandle = 1u;
    D3DGPU_VIRTUAL_ADDRESS fenceGpuAddress = 0;
    osContext.resetMonitoredFenceParams(fenceHandle, &fenceValue, fenceGpuAddress);

    auto &waitData = osContext.getMonitoredFenceKmdWaitData();
    waitData.eventHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(1));
    waitData.pendingFenceValue = 1u;

    WaitStatus firstWaitStatus = WaitStatus::ready;
    std::thread firstWaitThread([&] {
        firstWaitStatus = wddm.Wddm::waitFromCpu(1u, osContext, 10000000u);
    });

    while (!wddm.firstWaitEntered) {
        std::this_thread::yield();
    }

    fenceValue = 1u;
    const auto secondWaitStatus = wddm.Wddm::waitFromCpu(2u, osContext, 10000000u);
    const auto waitCallsBeforeFinishingFirstWait = wddm.waitCalled.load();
    const auto resetCallsBeforeFinishingFirstWait = wddm.resetCalled.load();

    wddm.finishFirstWait = true;
    firstWaitThread.join();

    EXPECT_EQ(WaitStatus::ready, firstWaitStatus);
    EXPECT_EQ(WaitStatus::notReady, secondWaitStatus);
    EXPECT_EQ(1u, waitCallsBeforeFinishingFirstWait);
    EXPECT_EQ(0u, resetCallsBeforeFinishingFirstWait);

    waitData.eventHandle = nullptr;
    waitData.pendingFenceValue = 0;
}

namespace {
std::atomic<uint32_t> createdKmdWaitEvents = 0;

HANDLE countingCreateEventMock(LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCSTR, void *) {
    createdKmdWaitEvents++;
    return reinterpret_cast<HANDLE>(static_cast<uintptr_t>(1));
}

NTSTATUS __stdcall registerKmdWaitMock(const D3DKMT_WAITFORSYNCHRONIZATIONOBJECTFROMCPU *) {
    return STATUS_SUCCESS;
}
} // namespace

TEST(WddmKmdWaitMTTest, givenConcurrentKmdWaitersWhenTheyAreCreatedAndDestroyedThenEachHandlePairIsCachedExactlyOnce) {
    MockExecutionEnvironment executionEnvironment;
    WddmMock wddm(*executionEnvironment.rootDeviceEnvironments[0]);
    wddm.getGdi()->waitForSynchronizationObjectFromCpu = &registerKmdWaitMock;
    VariableBackup<decltype(mockCreateEventClb)> createEventBackup(&mockCreateEventClb, &countingCreateEventMock);
    createdKmdWaitEvents = 0;

    volatile uint64_t fenceCpuValue = 0u;
    MonitoredFence monitoredFence = {};
    monitoredFence.cpuAddress = &fenceCpuValue;

    constexpr uint32_t threadCount = 4;
    constexpr uint32_t waitersPerThread = 1000;
    std::atomic<uint32_t> createdWaiters = 0;
    std::vector<std::thread> threads;
    for (uint32_t threadIndex = 0; threadIndex < threadCount; threadIndex++) {
        threads.emplace_back([&] {
            for (uint32_t iteration = 0; iteration < waitersPerThread; iteration++) {
                if (wddm.Wddm::createMonitoredFenceKmdWaiter(monitoredFence, 1u)) {
                    createdWaiters++;
                }
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_EQ(threadCount * waitersPerThread, createdWaiters);
    EXPECT_LE(createdKmdWaitEvents, threadCount);
    EXPECT_EQ(createdKmdWaitEvents, wddm.unusedKmdWaitHandles.size());
}
