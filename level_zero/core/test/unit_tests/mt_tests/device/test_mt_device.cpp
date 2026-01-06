/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/in_order_cmd_list_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include <array>

namespace L0 {
namespace ult {

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

using AggregatedBcsSplitMtTests = AggregatedBcsSplitTests;

HWTEST2_F(AggregatedBcsSplitMtTests, givenBcsSplitEnabledWhenMultipleThreadsAccessingThenInternalResourcesUsedCorrectly, IsAtLeastXeHpcCore) {
    constexpr uint32_t numThreads = 8;
    constexpr uint32_t iterationCount = 5;

    std::array<DestroyableZeUniquePtr<L0::CommandList>, numThreads> cmdLists = {};
    std::array<std::thread, numThreads> threads = {};
    std::array<void *, numThreads> hostPtrs = {};
    std::vector<TaskCountType> initialTaskCounts;

    for (uint32_t i = 0; i < numThreads; i++) {
        cmdLists[i] = createCmdList(true);
        hostPtrs[i] = allocHostMem();
        cmdLists[i]->appendMemoryCopy(hostPtrs[i], hostPtrs[i], copySize, nullptr, 0, nullptr, copyParams);
    }

    for (auto &cmdList : bcsSplit->cmdLists) {
        initialTaskCounts.push_back(cmdList->getCsr(false)->peekTaskCount());
    }

    std::atomic_bool started = false;

    auto threadBody = [&](uint32_t cmdListId) {
        while (!started.load()) {
            std::this_thread::yield();
        }

        auto localCopyParams = copyParams;

        for (uint32_t i = 1; i < iterationCount; i++) {
            cmdLists[cmdListId]->appendMemoryCopy(hostPtrs[cmdListId], hostPtrs[cmdListId], copySize, nullptr, 0, nullptr, localCopyParams);
        }
    };

    for (uint32_t i = 0; i < numThreads; ++i) {
        threads[i] = std::thread(threadBody, i);
    }

    started = true;

    for (auto &thread : threads) {
        thread.join();
    }

    for (size_t i = 0; i < bcsSplit->cmdLists.size(); i++) {
        EXPECT_TRUE(bcsSplit->cmdLists[i]->getCsr(false)->peekTaskCount() > initialTaskCounts[i]);
    }

    for (auto &ptr : hostPtrs) {
        context->freeMem(ptr);
    }
}
} // namespace ult
} // namespace L0
