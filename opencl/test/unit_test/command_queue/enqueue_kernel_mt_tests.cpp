/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_submissions_aggregator.h"

#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"

typedef HelloWorldFixture<HelloWorldFixtureFactory> EnqueueKernelFixture;
typedef Test<EnqueueKernelFixture> EnqueueKernelTest;

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenFinishIsCalledThenBatchesSubmissionsAreFlushed) {
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    std::atomic<bool> startEnqueueProcess(false);

    MockKernelWithInternals mockKernel(*pClDevice);
    size_t gws[3] = {1, 0, 0};

    auto enqueueCount = 10;
    auto threadCount = 4;

    auto function = [&]() {
        //wait until we are signalled
        while (!startEnqueueProcess)
            ;
        for (int enqueue = 0; enqueue < enqueueCount; enqueue++) {
            pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
        }
    };

    std::vector<std::thread> threads;
    for (auto thread = 0; thread < threadCount; thread++) {
        threads.push_back(std::thread(function));
    }

    auto currentTaskCount = 0;

    startEnqueueProcess = true;

    //call a flush while other threads enqueue, we can't drop anything
    while (currentTaskCount < enqueueCount * threadCount) {
        clFlush(pCmdQ);
        auto locker = mockCsr->obtainUniqueOwnership();
        currentTaskCount = mockCsr->peekTaskCount();
    }

    for (auto &thread : threads) {
        thread.join();
    }

    pCmdQ->finish();

    EXPECT_GE(mockCsr->flushCalledCount, 1);

    EXPECT_LE(mockCsr->flushCalledCount, enqueueCount * threadCount);

    EXPECT_EQ(mockedSubmissionsAggregator->peekInspectionId() - 1, (uint32_t)mockCsr->flushCalledCount);
}
