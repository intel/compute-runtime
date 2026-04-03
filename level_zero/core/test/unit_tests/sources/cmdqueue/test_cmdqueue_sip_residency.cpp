/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

struct SipResidencyFixture : DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        ze_result_t returnValue;
        commandListHandle = CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle();
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        CommandList::fromHandle(commandListHandle)->close();

        ze_command_queue_desc_t queueDesc{};
        cmdQ = CommandQueue::create(productFamily, device,
                                    neoDevice->getDefaultEngine().commandStreamReceiver,
                                    &queueDesc, false, false, false, returnValue);
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        commandQueue = whiteboxCast(cmdQ);
    }

    void tearDown() {
        *neoDevice->getDefaultEngine().commandStreamReceiver->getTagAddress() = std::numeric_limits<TaskCountType>::max();
        CommandList::fromHandle(commandListHandle)->destroy();
        cmdQ->destroy();
        DeviceFixture::tearDown();
    }

    ze_command_list_handle_t commandListHandle = nullptr;
    L0::CommandQueue *cmdQ = nullptr;
    L0::ult::CommandQueue *commandQueue = nullptr;
};

struct SipResidencyDebuggerFixture : L0DebuggerHwFixture {
    void setUp() {
        L0DebuggerHwFixture::setUp();
        ze_result_t returnValue;
        commandListHandle = CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle();
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        CommandList::fromHandle(commandListHandle)->close();

        ze_command_queue_desc_t queueDesc{};
        cmdQ = CommandQueue::create(productFamily, device,
                                    neoDevice->getDefaultEngine().commandStreamReceiver,
                                    &queueDesc, false, false, false, returnValue);
        ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
        commandQueue = whiteboxCast(cmdQ);
    }

    void tearDown() {
        *neoDevice->getDefaultEngine().commandStreamReceiver->getTagAddress() = std::numeric_limits<TaskCountType>::max();
        CommandList::fromHandle(commandListHandle)->destroy();
        cmdQ->destroy();
        L0DebuggerHwFixture::tearDown();
    }

    ze_command_list_handle_t commandListHandle = nullptr;
    L0::CommandQueue *cmdQ = nullptr;
    L0::ult::CommandQueue *commandQueue = nullptr;
};

using CommandQueueSipResidencyTest = Test<SipResidencyFixture>;
using CommandQueueSipResidencyDebuggerTest = Test<SipResidencyDebuggerFixture>;

HWTEST_F(CommandQueueSipResidencyTest, givenNeitherPreemptionNorDebuggerActiveWhenExecutingCommandListThenNoSipAllocationIsCached) {
    neoDevice->setPreemptionMode(NEO::PreemptionMode::ThreadGroup);

    EXPECT_EQ(nullptr, commandQueue->cachedSipAllocation);

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

    EXPECT_EQ(nullptr, commandQueue->cachedSipAllocation);
}

HWTEST_F(CommandQueueSipResidencyTest, givenMidThreadPreemptionActiveWhenExecutingCommandListThenSipAllocationIsCached) {
    neoDevice->setPreemptionMode(NEO::PreemptionMode::MidThread);
    auto *csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    if (!csr->getPreemptionAllocation()) {
        csr->createPreemptionAllocation();
    }

    EXPECT_EQ(nullptr, commandQueue->cachedSipAllocation);

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

    EXPECT_NE(nullptr, commandQueue->cachedSipAllocation);
}

HWTEST_F(CommandQueueSipResidencyTest, givenCachedSipAllocationWhenExecutingCommandListAgainThenSameAllocationPointerIsReused) {
    neoDevice->setPreemptionMode(NEO::PreemptionMode::MidThread);
    auto *csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    if (!csr->getPreemptionAllocation()) {
        csr->createPreemptionAllocation();
    }

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    ASSERT_NE(nullptr, commandQueue->cachedSipAllocation);
    auto *cachedAfterFirstExecute = commandQueue->cachedSipAllocation;

    *csr->getTagAddress() = std::numeric_limits<TaskCountType>::max();

    CommandList::fromHandle(commandListHandle)->reset();
    CommandList::fromHandle(commandListHandle)->close();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

    EXPECT_EQ(cachedAfterFirstExecute, commandQueue->cachedSipAllocation);
}

HWTEST_F(CommandQueueSipResidencyDebuggerTest, givenNEODebuggerActiveWhenExecutingCommandListThenSipAllocationIsCached) {
    EXPECT_EQ(nullptr, commandQueue->cachedSipAllocation);

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

    EXPECT_NE(nullptr, commandQueue->cachedSipAllocation);
}

} // namespace ult
} // namespace L0
