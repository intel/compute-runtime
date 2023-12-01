/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

using namespace NEO;

TEST(CommandQueue, givenCommandQueueWhenTakeOwnershipWrapperForCommandQueueThenWaitForTimestampsWaitsToBeUnlocked) {
    DebugManagerStateRestore restorer;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useWaitForTimestamps = true;
    debugManager.flags.EnableTimestampWaitForQueues.set(4);

    auto pDevice = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    MockContext context(pDevice.get());
    cl_int retVal = CL_SUCCESS;
    cl_queue_properties propertiesQueue[5] = {};

    auto pCmdQ = CommandQueue::create(
        &context,
        pDevice.get(),
        propertiesQueue,
        false,
        retVal);

    auto status = WaitStatus::notReady;

    TakeOwnershipWrapper<CommandQueue> queueOwnership(*pCmdQ);
    std::atomic<bool> threadStarted = false;
    std::atomic<bool> threadFinished = false;

    std::thread t([&]() {
        threadStarted = true;
        pCmdQ->waitForTimestamps({}, status, const_cast<TimestampPacketContainer *>(pCmdQ->getTimestampPacketContainer()), const_cast<TimestampPacketContainer *>(pCmdQ->getTimestampPacketContainer()));
        threadFinished = true;
    });
    while (!threadStarted)
        ;
    EXPECT_FALSE(threadFinished);
    queueOwnership.unlock();
    t.join();
    EXPECT_TRUE(threadFinished);
    delete pCmdQ;
}
