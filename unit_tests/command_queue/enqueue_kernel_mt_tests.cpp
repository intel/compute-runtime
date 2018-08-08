/*
* Copyright (c) 2017 - 2018, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"

typedef HelloWorldFixture<HelloWorldFixtureFactory> EnqueueKernelFixture;
typedef Test<EnqueueKernelFixture> EnqueueKernelTest;

HWTEST_F(EnqueueKernelTest, givenCsrInBatchingModeWhenFinishIsCalledThenBatchesSubmissionsAreFlushed) {
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);

    mockCsr->overrideDispatchPolicy(DispatchMode::BatchedDispatch);
    pDevice->resetCommandStreamReceiver(mockCsr);

    auto mockedSubmissionsAggregator = new mockSubmissionsAggregator();
    mockCsr->overrideSubmissionAggregator(mockedSubmissionsAggregator);

    std::atomic<bool> startEnqueueProcess(false);

    MockKernelWithInternals mockKernel(*pDevice);
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

    pCmdQ->finish(false);

    EXPECT_GE(mockCsr->flushCalledCount, 1);

    EXPECT_LE(mockCsr->flushCalledCount, enqueueCount * threadCount);

    EXPECT_EQ(mockedSubmissionsAggregator->peekInspectionId() - 1, (uint32_t)mockCsr->flushCalledCount);
}
