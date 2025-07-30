/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

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
using DeviceMtTest = Test<DeviceFixture>;
HWTEST_F(DeviceMtTest, givenMultiThreadsExecutingCmdListAndSynchronizingDeviceWhenSynchronizeIsCalledThenTaskCountAndFlushStampAreTakenWithinSingleCriticalSection) {
    L0::Device *device = driverHandle->devices[0];

    auto csr = static_cast<UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);
    csr->latestSentTaskCount = 0u;
    csr->latestFlushedTaskCount = 0u;
    csr->taskCount = 0;
    csr->flushStamp->setStamp(0);
    csr->captureWaitForTaskCountWithKmdNotifyInputParams = true;
    csr->waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::ready;
    csr->resourcesInitialized = true;
    csr->incrementFlushStampOnFlush = true;

    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(defaultHwInfo->platform.eProductFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    ASSERT_NE(nullptr, commandQueue);

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    kernel.immutableData.device = device;

    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(defaultHwInfo->platform.eProductFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);

    ze_group_count_t dispatchKernelArguments{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel.toHandle(), dispatchKernelArguments, nullptr, 0, nullptr, launchParams);

    commandList->close();

    std::atomic_bool started = false;
    constexpr int numThreads = 8;
    constexpr int iterationCount = 20;
    std::vector<std::thread> threads;

    auto threadBody = [&]() {
        ze_command_list_handle_t cmdList = commandList->toHandle();

        for (auto i = 0; i < iterationCount; i++) {
            commandQueue->executeCommandLists(1, &cmdList, nullptr, false, nullptr, nullptr);
            device->synchronize();
        }
    };

    for (int i = 0; i < numThreads; ++i) {
        threads.push_back(std::thread(threadBody));
    }

    started = true;

    for (auto &thread : threads) {
        thread.join();
    }

    auto expectedWaitCalls = numThreads * iterationCount;
    EXPECT_EQ(static_cast<size_t>(expectedWaitCalls), csr->waitForTaskCountWithKmdNotifyInputParams.size());

    for (auto i = 0; i < static_cast<int>(csr->waitForTaskCountWithKmdNotifyInputParams.size()); i++) {
        auto &inputParams = csr->waitForTaskCountWithKmdNotifyInputParams[i];
        EXPECT_NE(0u, inputParams.taskCountToWait);
        EXPECT_NE(0u, inputParams.flushStampToWait);
        EXPECT_EQ(inputParams.taskCountToWait, inputParams.flushStampToWait);
    }
    commandQueue->destroy();
}
} // namespace ult
} // namespace L0
