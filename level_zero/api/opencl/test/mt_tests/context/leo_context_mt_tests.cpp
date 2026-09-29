/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/command_list_create_immediate_hook.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct LeoContextMtTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        cl_device_id clDeviceId = platform->getDevices()[0].get();
        leoContext = std::make_unique<Context>(nullptr, context->toHandle(), 1, &clDeviceId, true);
        ASSERT_EQ(CL_SUCCESS, leoContext->initialize());
    }

    void TearDown() override {
        leoContext.reset();
        Test<OclFixture>::TearDown();
    }

    std::unique_ptr<Context> leoContext;
};

TEST_F(LeoContextMtTest, givenConcurrentRequestsForInternalCmdListsWhenGettingThemThenEachIsCreatedOnceAndAllThreadsGetTheSameHandle) {
    CommandListCreateImmediateHook createImmediateHook;
    constexpr size_t numThreads = 8u;

    std::vector<ze_command_list_handle_t> copyCmdLists(numThreads, nullptr);
    std::vector<ze_command_list_handle_t> computeCmdLists(numThreads, nullptr);
    std::vector<ze_result_t> copyResults(numThreads, ZE_RESULT_ERROR_UNKNOWN);
    std::vector<ze_result_t> computeResults(numThreads, ZE_RESULT_ERROR_UNKNOWN);
    std::atomic<bool> startRequests{false};

    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    for (size_t threadIndex = 0; threadIndex < numThreads; threadIndex++) {
        threads.emplace_back([&, threadIndex]() {
            while (!startRequests.load()) {
                std::this_thread::yield();
            }
            copyResults[threadIndex] = leoContext->getInternalCopyCmdList(copyCmdLists[threadIndex]);
            computeResults[threadIndex] = leoContext->getInternalComputeCmdList(computeCmdLists[threadIndex]);
        });
    }
    startRequests.store(true);
    for (auto &thread : threads) {
        thread.join();
    }

    ASSERT_NE(nullptr, copyCmdLists[0]);
    ASSERT_NE(nullptr, computeCmdLists[0]);
    EXPECT_NE(copyCmdLists[0], computeCmdLists[0]);
    for (size_t threadIndex = 0; threadIndex < numThreads; threadIndex++) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, copyResults[threadIndex]);
        EXPECT_EQ(ZE_RESULT_SUCCESS, computeResults[threadIndex]);
        EXPECT_EQ(copyCmdLists[0], copyCmdLists[threadIndex]);
        EXPECT_EQ(computeCmdLists[0], computeCmdLists[threadIndex]);
    }
    EXPECT_EQ(2u, createImmediateHook.requestedFlags.size());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
