/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/tracing/leo_tracing_api.h"
#include "level_zero/api/opencl/source/tracing/leo_tracing_handle.h"
#include "level_zero/api/opencl/source/tracing/leo_tracing_notify.h"

#include "CL/cl.h"

#include <array>
#include <atomic>
#include <thread>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

using namespace HostSideTracing;

static void emptyTracingCallback(ClFunctionId, cl_callback_data *, void *) {}

struct TracingMtFixture : public ::testing::Test {
    void SetUp() override {
        savedState = tracingState.load();
        for (size_t i = 0; i < tracingMaxHandleCount; i++) {
            savedHandles[i] = tracingHandle[i];
            tracingHandle[i] = nullptr;
        }
        tracingState.store(0u);
    }

    void TearDown() override {
        for (auto handle : createdHandles) {
            clDisableTracingINTEL(handle);
            clDestroyTracingHandleINTEL(handle);
        }
        for (size_t i = 0; i < tracingMaxHandleCount; i++) {
            tracingHandle[i] = savedHandles[i];
        }
        tracingState.store(savedState);
    }

    cl_tracing_handle createHandle() {
        cl_tracing_handle handle = nullptr;
        EXPECT_EQ(CL_SUCCESS, clCreateTracingHandleINTEL(fakeDevice, &emptyTracingCallback, nullptr, &handle));
        EXPECT_NE(nullptr, handle);
        createdHandles.push_back(handle);
        return handle;
    }

    static size_t countEnabledSlots() {
        size_t enabled = 0u;
        while (enabled < tracingMaxHandleCount && tracingHandle[enabled] != nullptr) {
            ++enabled;
        }
        return enabled;
    }

    cl_device_id fakeDevice = reinterpret_cast<cl_device_id>(0x1234u);
    std::vector<cl_tracing_handle> createdHandles{};
    std::array<TracingHandle *, tracingMaxHandleCount> savedHandles{};
    uint32_t savedState = 0u;
};

TEST_F(TracingMtFixture, givenConcurrentClientsWhenTheyAllFinishThenTheClientCounterIsBackToZero) {
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(createHandle()));

    constexpr uint32_t numThreads = 4u;
    constexpr uint32_t iterationsPerThread = 2000u;
    std::atomic<uint32_t> failedAdds{0};

    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < numThreads; i++) {
        workers.emplace_back([&]() {
            for (uint32_t iteration = 0; iteration < iterationsPerThread; iteration++) {
                if (!addTracingClient()) {
                    failedAdds++;
                    continue;
                }
                removeTracingClient();
            }
        });
    }
    for (auto &worker : workers) {
        worker.join();
    }

    EXPECT_EQ(0u, failedAdds.load());
    EXPECT_EQ(0u, TRACING_GET_CLIENT_COUNTER(tracingState.load()));
    EXPECT_NE(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
    EXPECT_EQ(0u, TRACING_GET_LOCKED_BIT(tracingState.load()));
}

TEST_F(TracingMtFixture, givenTracingDisabledWhenClientsAreAddedConcurrentlyThenNoneOfThemSucceeds) {
    constexpr uint32_t numThreads = 4u;
    constexpr uint32_t iterationsPerThread = 500u;
    std::atomic<uint32_t> succeededAdds{0};

    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < numThreads; i++) {
        workers.emplace_back([&]() {
            for (uint32_t iteration = 0; iteration < iterationsPerThread; iteration++) {
                if (addTracingClient()) {
                    succeededAdds++;
                    removeTracingClient();
                }
            }
        });
    }
    for (auto &worker : workers) {
        worker.join();
    }

    EXPECT_EQ(0u, succeededAdds.load());
    EXPECT_EQ(0u, TRACING_GET_CLIENT_COUNTER(tracingState.load()));
}

TEST_F(TracingMtFixture, givenDistinctHandlesEnabledConcurrentlyThenEachOfThemTakesItsOwnSlot) {
    constexpr uint32_t numThreads = 8u;
    std::vector<cl_tracing_handle> handles;
    for (uint32_t i = 0; i < numThreads; i++) {
        handles.push_back(createHandle());
    }

    std::atomic<uint32_t> enabledCount{0};
    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < numThreads; i++) {
        workers.emplace_back([&, i]() {
            if (CL_SUCCESS == clEnableTracingINTEL(handles[i])) {
                enabledCount++;
            }
        });
    }
    for (auto &worker : workers) {
        worker.join();
    }

    EXPECT_EQ(numThreads, enabledCount.load());
    EXPECT_EQ(numThreads, countEnabledSlots());
    EXPECT_NE(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));

    for (auto handle : handles) {
        cl_bool enabled = CL_FALSE;
        EXPECT_EQ(CL_SUCCESS, clGetTracingStateINTEL(handle, &enabled));
        EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), enabled);
    }
}

TEST_F(TracingMtFixture, givenTheSameHandleEnabledConcurrentlyThenOnlyOneAttemptSucceeds) {
    constexpr uint32_t numThreads = 8u;
    auto handle = createHandle();

    std::atomic<uint32_t> enabledCount{0};
    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < numThreads; i++) {
        workers.emplace_back([&]() {
            if (CL_SUCCESS == clEnableTracingINTEL(handle)) {
                enabledCount++;
            }
        });
    }
    for (auto &worker : workers) {
        worker.join();
    }

    EXPECT_EQ(1u, enabledCount.load());
    EXPECT_EQ(1u, countEnabledSlots());
}

TEST_F(TracingMtFixture, givenEnabledHandlesDisabledConcurrentlyThenAllSlotsAreFreedAndTracingIsTurnedOff) {
    constexpr uint32_t numThreads = 8u;
    std::vector<cl_tracing_handle> handles;
    for (uint32_t i = 0; i < numThreads; i++) {
        handles.push_back(createHandle());
        ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handles.back()));
    }
    ASSERT_EQ(numThreads, countEnabledSlots());

    std::atomic<uint32_t> disabledCount{0};
    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < numThreads; i++) {
        workers.emplace_back([&, i]() {
            if (CL_SUCCESS == clDisableTracingINTEL(handles[i])) {
                disabledCount++;
            }
        });
    }
    for (auto &worker : workers) {
        worker.join();
    }

    EXPECT_EQ(numThreads, disabledCount.load());
    EXPECT_EQ(0u, countEnabledSlots());
    EXPECT_EQ(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
    EXPECT_EQ(0u, TRACING_GET_LOCKED_BIT(tracingState.load()));
}

TEST_F(TracingMtFixture, givenConcurrentEnableAndDisableOfTheSameHandleThenTheStateStaysConsistent) {
    constexpr uint32_t numThreads = 4u;
    constexpr uint32_t iterationsPerThread = 500u;
    auto handle = createHandle();

    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < numThreads; i++) {
        workers.emplace_back([&]() {
            for (uint32_t iteration = 0; iteration < iterationsPerThread; iteration++) {
                clEnableTracingINTEL(handle);
                clDisableTracingINTEL(handle);
            }
        });
    }
    for (auto &worker : workers) {
        worker.join();
    }

    EXPECT_GE(1u, countEnabledSlots());
    EXPECT_EQ(0u, TRACING_GET_LOCKED_BIT(tracingState.load()));
    EXPECT_EQ(0u, TRACING_GET_CLIENT_COUNTER(tracingState.load()));

    clDisableTracingINTEL(handle);
    EXPECT_EQ(0u, countEnabledSlots());
    EXPECT_EQ(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
}

TEST_F(TracingMtFixture, givenTracingStateQueriedConcurrentlyWithEnableAndDisableThenEveryQuerySucceeds) {
    constexpr uint32_t queryIterations = 2000u;
    auto togglingHandle = createHandle();
    auto queriedHandle = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(queriedHandle));

    std::atomic<bool> stopToggling{false};
    std::thread toggler([&]() {
        while (!stopToggling.load()) {
            clEnableTracingINTEL(togglingHandle);
            clDisableTracingINTEL(togglingHandle);
        }
    });

    std::atomic<uint32_t> failedQueries{0};
    std::atomic<uint32_t> disabledQueries{0};
    for (uint32_t iteration = 0; iteration < queryIterations; iteration++) {
        cl_bool enabled = CL_FALSE;
        if (CL_SUCCESS != clGetTracingStateINTEL(queriedHandle, &enabled)) {
            failedQueries++;
        } else if (CL_TRUE != enabled) {
            disabledQueries++;
        }
    }

    stopToggling.store(true);
    toggler.join();

    EXPECT_EQ(0u, failedQueries.load());
    EXPECT_EQ(0u, disabledQueries.load());
    EXPECT_EQ(0u, TRACING_GET_LOCKED_BIT(tracingState.load()));
}

TEST_F(TracingMtFixture, givenClientsAddedConcurrentlyWithHandleEnablingThenTheLockIsAlwaysReleased) {
    constexpr uint32_t iterations = 1000u;
    auto enabledHandle = createHandle();
    auto togglingHandle = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(enabledHandle));

    std::atomic<bool> stopToggling{false};
    std::thread toggler([&]() {
        while (!stopToggling.load()) {
            clEnableTracingINTEL(togglingHandle);
            clDisableTracingINTEL(togglingHandle);
        }
    });

    for (uint32_t iteration = 0; iteration < iterations; iteration++) {
        if (addTracingClient()) {
            removeTracingClient();
        }
    }

    stopToggling.store(true);
    toggler.join();

    EXPECT_EQ(0u, TRACING_GET_CLIENT_COUNTER(tracingState.load()));
    EXPECT_EQ(0u, TRACING_GET_LOCKED_BIT(tracingState.load()));
    EXPECT_NE(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
