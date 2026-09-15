/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_os_context_win.h"
#include "shared/test/common/mocks/mock_wddm.h"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>

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
