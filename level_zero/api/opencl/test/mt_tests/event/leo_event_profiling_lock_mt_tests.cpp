/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/test/common/fixtures/leo_event_callbacks_fixture.h"

#include <mutex>

namespace NEO {
namespace LEO {
namespace ult {

using EventProfilingLockMtTests = Test<EventCallbacksFixture>;

struct MockProfilingEvent : public Event {
    using Event::dataCalculated;
    using Event::Event;
    using Event::setQueueTimeStamp;
    using Event::setSubmitTimeStamp;
    using Event::submitTimeStamp;

    ze_result_t queryKernelTimestamp(ze_kernel_timestamp_result_t &result) override {
        result.global.kernelStart = injectedKernelStart;
        result.global.kernelEnd = injectedKernelEnd;
        queryCount++;

        // Probe from another thread: a recursive_mutex try_lock only fails here if some other
        // thread owns it, so this distinguishes "the query holds ownership" from "it does not".
        std::thread probe([this] {
            std::unique_lock<std::recursive_mutex> probeLock(this->mtx, std::try_to_lock);
            this->ownershipAvailable = probeLock.owns_lock();
        });
        probe.join();

        return ZE_RESULT_SUCCESS;
    }

    uint64_t injectedKernelStart = 0;
    uint64_t injectedKernelEnd = 0;
    std::atomic<uint32_t> queryCount{0};
    std::atomic<bool> ownershipAvailable{false};
};

TEST_F(EventProfilingLockMtTests, givenProfilingQueryWhenResolvingTimestampThenEventOwnershipIsNotHeld) {
    auto event = std::make_unique<MockProfilingEvent>(CL_COMMAND_NDRANGE_KERNEL, commandQueue);
    event->setQueueTimeStamp();
    event->setSubmitTimeStamp();
    event->injectedKernelStart = event->submitTimeStamp.gpuTimeStamp + 0x1000;
    event->injectedKernelEnd = event->injectedKernelStart + 0x500;

    cl_ulong timestamp = 0;
    EXPECT_EQ(CL_SUCCESS, event->getProfilingInfo(CL_PROFILING_COMMAND_START, sizeof(timestamp), &timestamp, nullptr));

    // The query may spin waiting for the timestamp writeback, so it must not run under the lock.
    EXPECT_EQ(1u, event->queryCount);
    EXPECT_TRUE(event->ownershipAvailable);
}

TEST_F(EventProfilingLockMtTests, givenConcurrentProfilingQueriesWhenDerivingDataThenAnchorsAreRebasedOnce) {
    auto event = std::make_unique<MockProfilingEvent>(CL_COMMAND_NDRANGE_KERNEL, commandQueue);
    event->setQueueTimeStamp();
    event->setSubmitTimeStamp();
    event->injectedKernelStart = event->submitTimeStamp.gpuTimeStamp + 0x1000;
    event->injectedKernelEnd = event->injectedKernelStart + 0x500;

    // Ownership still serialises the derivation, so both callers observe the same rebased anchor
    // no matter which of them wins the race to compute it.
    constexpr uint32_t numThreads = 4u;
    cl_ulong observed[numThreads]{};
    std::vector<std::thread> threads;
    for (uint32_t i = 0; i < numThreads; i++) {
        threads.emplace_back([&, i] {
            EXPECT_EQ(CL_SUCCESS, event->getProfilingInfo(CL_PROFILING_COMMAND_START, sizeof(observed[i]), &observed[i], nullptr));
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }

    EXPECT_TRUE(event->dataCalculated);
    for (uint32_t i = 1; i < numThreads; i++) {
        EXPECT_EQ(observed[0], observed[i]);
    }
}

} // namespace ult
} // namespace LEO
} // namespace NEO
