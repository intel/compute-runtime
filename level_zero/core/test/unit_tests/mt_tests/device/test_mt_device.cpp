/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"

namespace L0 {
namespace ult {

using MultiDeviceMtTest = Test<MultiDeviceFixture>;

TEST_F(MultiDeviceMtTest, givenTwoDevicesWhenCanAccessPeerIsCalledManyTimesFromMultiThreadsInBothWaysThenPeerAccessIsQueriedOnlyOnce) {
    L0::Device *device0 = driverHandle->devices[0];
    L0::Device *device1 = driverHandle->devices[1];

    auto taskCount0 = device0->getNEODevice()->getInternalEngine().commandStreamReceiver->peekLatestFlushedTaskCount();
    auto taskCount1 = device1->getNEODevice()->getInternalEngine().commandStreamReceiver->peekLatestFlushedTaskCount();

    EXPECT_EQ(taskCount0, taskCount1);
    EXPECT_EQ(0u, taskCount0);

    std::atomic_bool started = false;
    constexpr int numThreads = 8;
    constexpr int iterationCount = 20;
    std::vector<std::thread> threads;

    auto threadBody = [&](int threadId) {
        while (!started.load()) {
            std::this_thread::yield();
        }

        auto device = device0;
        auto peerDevice = device1;

        if (threadId & 1) {
            device = device1;
            peerDevice = device0;
        }
        for (auto i = 0; i < iterationCount; i++) {
            ze_bool_t canAccess = false;
            ze_result_t res = device->canAccessPeer(peerDevice->toHandle(), &canAccess);
            EXPECT_EQ(ZE_RESULT_SUCCESS, res);
            EXPECT_TRUE(canAccess);
        }
    };

    for (int i = 0; i < numThreads; ++i) {
        threads.push_back(std::thread(threadBody, i));
    }

    started = true;

    for (auto &thread : threads) {
        thread.join();
    }

    taskCount0 = device0->getNEODevice()->getInternalEngine().commandStreamReceiver->peekLatestFlushedTaskCount();
    taskCount1 = device1->getNEODevice()->getInternalEngine().commandStreamReceiver->peekLatestFlushedTaskCount();

    EXPECT_NE(taskCount0, taskCount1);

    EXPECT_GE(2u, std::max(taskCount0, taskCount1));
    EXPECT_EQ(0u, std::min(taskCount0, taskCount1));
}
} // namespace ult
} // namespace L0
