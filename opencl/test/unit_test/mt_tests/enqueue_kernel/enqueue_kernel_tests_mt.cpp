/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"

using namespace NEO;

typedef HelloWorldTest<HelloWorldFixtureFactory> EnqueueKernelTestsMt;

TEST_F(EnqueueKernelTestsMt, WhenCallEnqueueKernelsThenAllCallsPass) {
    size_t workSize[] = {1};
    const int iterNum = 2;
    const int threadNum = 4;
    const int taskNum = 4;
    std::atomic<int> result = {0};

    for (int iter = 0; iter < iterNum; iter++) {
        std::vector<std::thread> threads;
        for (int thr = 0; thr < threadNum; thr++) {
            threads.emplace_back([&]() {
                for (int j = 0; j < taskNum; j++) {
                    result += EnqueueKernelHelper<>::enqueueKernel(pCmdQ,
                                                                   KernelFixture::pKernel,
                                                                   1,
                                                                   nullptr,
                                                                   workSize,
                                                                   workSize,
                                                                   0, nullptr, nullptr);
                    result += clFinish(pCmdQ);
                }
            });
        }
        for (auto &t : threads)
            t.join();
        EXPECT_EQ(CL_SUCCESS, result);
    }
}
