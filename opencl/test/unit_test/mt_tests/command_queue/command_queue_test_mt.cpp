/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/timestamp_packet_container.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/event/event.h"
#include "opencl/source/helpers/base_object.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue_hw.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "CL/cl.h"
#include "gtest/gtest.h"

#include <atomic>
#include <thread>

using namespace NEO;

TEST(CommandQueue, givenCommandQueueWhenTakeOwnershipWrapperForCommandQueueThenWaitForTimestampsDoesNotWaitForLock) {
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
    std::atomic<bool> threadFinished = false;

    std::thread t([&]() {
        pCmdQ->waitForTimestamps({}, status, const_cast<TimestampPacketContainer *>(pCmdQ->getTimestampPacketContainer()), const_cast<TimestampPacketContainer *>(pCmdQ->getTimestampPacketContainer()));
        threadFinished = true;
    });
    t.join();
    EXPECT_TRUE(threadFinished);
    queueOwnership.unlock();
    delete pCmdQ;
}

TEST(CommandQueue, givenCommandQueueWhenTakeOwnershipWrapperForCommandQueueThenWaitForAllEnginesWaitsToBeUnlocked) {
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

    TakeOwnershipWrapper<CommandQueue> queueOwnership(*pCmdQ);
    std::atomic<bool> threadStarted = false;
    std::atomic<bool> threadFinished = false;

    std::thread t([&]() {
        threadStarted = true;
        pCmdQ->waitForAllEngines(false, nullptr, false, false);
        threadFinished = true;
    });
    while (!threadStarted) {
        ;
    }
    EXPECT_FALSE(threadFinished);
    queueOwnership.unlock();
    t.join();
    EXPECT_TRUE(threadFinished);
    delete pCmdQ;
}

HWTEST_F(CommandQueueHwTest, givenEventWithRecordedCommandWhenSubmitCommandIsCalledThenTaskCountMustBeUpdatedFromOtherThread) {
    std::atomic_bool go{false};

    struct MockEvent : public Event {
        using Event::Event;
        using Event::eventWithoutCommand;
        using Event::submitCommand;
        void synchronizeTaskCount() override {
            *atomicFence = true;
            Event::synchronizeTaskCount();
        }
        uint32_t synchronizeCallCount = 0u;
        std::atomic_bool *atomicFence = nullptr;
    };

    MockEvent neoEvent(this->pCmdQ, CL_COMMAND_MAP_BUFFER, CompletionStamp::notReady, CompletionStamp::notReady);
    neoEvent.atomicFence = &go;
    EXPECT_TRUE(neoEvent.eventWithoutCommand);
    neoEvent.eventWithoutCommand = false;

    EXPECT_EQ(CompletionStamp::notReady, neoEvent.peekTaskCount());

    std::thread t([&]() {
        while (!go) {
        }
        neoEvent.updateTaskCount(77u, 0);
    });

    neoEvent.submitCommand(false);

    EXPECT_EQ(77u, neoEvent.peekTaskCount());
    t.join();
}
