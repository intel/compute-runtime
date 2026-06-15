/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"

#include "gtest/gtest.h"

#include <atomic>
#include <thread>

using namespace NEO;

TEST(DeviceCanAccessPeerMtTests, givenTwoDevicesWhenCanAccessPeerIsCalledManyTimesFromMultiThreadsInBothWaysThenPeerAccessIsQueriedOnlyOnce) {
    UltDeviceFactory deviceFactory{2, 0};
    auto device0 = deviceFactory.rootDevices[0];
    auto device1 = deviceFactory.rootDevices[1];

    std::atomic<uint32_t> queryCalled = 0;

    auto queryPeerAccess = [&queryCalled](Device &device, Device &peerDevice, GraphicsAllocation **probeAllocPtr, uint64_t *handle) -> bool {
        queryCalled++;
        return true;
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

        auto device = device0;
        auto peerDevice = device1;

        if (threadId & 1) {
            device = device1;
            peerDevice = device0;
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

    EXPECT_EQ(1u, queryCalled);
}
