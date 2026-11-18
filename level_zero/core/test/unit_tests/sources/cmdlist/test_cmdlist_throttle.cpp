/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/queue_throttle.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/cmdqueue/internal_queue_throttle_ext.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"

namespace L0 {
namespace ult {

using CommandListThrottleTests = Test<CommandListCreateFixture>;

HWTEST_F(CommandListThrottleTests, givenImmediateCommandListWithoutThrottleExtensionWhenCreatedThenThrottleIsMedium) {
    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.pNext = nullptr;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device,
                                                                              &desc,
                                                                              false,
                                                                              NEO::EngineGroupType::renderCompute,
                                                                              returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());
    EXPECT_EQ(NEO::QueueThrottle::MEDIUM, whiteBoxCmdList->queueThrottle);
}

HWTEST_F(CommandListThrottleTests, givenImmediateCommandListWithLowThrottleExtensionWhenCreatedThenThrottleIsLow) {
    ze_queue_throttle_ext_desc_t throttleDesc;
    throttleDesc.throttle = NEO::QueueThrottle::LOW;

    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.pNext = &throttleDesc;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device,
                                                                              &desc,
                                                                              false,
                                                                              NEO::EngineGroupType::renderCompute,
                                                                              returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());
    EXPECT_EQ(NEO::QueueThrottle::LOW, whiteBoxCmdList->queueThrottle);
}

HWTEST_F(CommandListThrottleTests, givenImmediateCommandListWithHighThrottleExtensionWhenCreatedThenThrottleIsHigh) {
    ze_queue_throttle_ext_desc_t throttleDesc;
    throttleDesc.throttle = NEO::QueueThrottle::HIGH;

    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.pNext = &throttleDesc;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device,
                                                                              &desc,
                                                                              false,
                                                                              NEO::EngineGroupType::renderCompute,
                                                                              returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());
    EXPECT_EQ(NEO::QueueThrottle::HIGH, whiteBoxCmdList->queueThrottle);
}

struct CommandListThrottleHostSynchronizeTests : public CommandListThrottleTests {
    std::unique_ptr<L0::CommandList> createImmediateCmdList(NEO::QueueThrottle throttle) {
        ze_queue_throttle_ext_desc_t throttleDesc;
        throttleDesc.throttle = throttle;

        ze_command_queue_desc_t desc = {};
        desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
        desc.pNext = &throttleDesc;

        ze_result_t returnValue;
        std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(device,
                                                                                  &desc,
                                                                                  false,
                                                                                  NEO::EngineGroupType::renderCompute,
                                                                                  returnValue));
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
        return commandList;
    }

    template <typename FamilyType>
    NEO::UltCommandStreamReceiver<FamilyType> *prepareCsrForWait(L0::CommandList *commandList) {
        auto whiteBoxCmdList = CommandList::whiteboxCast(commandList);
        whiteBoxCmdList->cmdQImmediate->setTaskCount(1);

        auto csr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(commandList->getCsr(false));
        csr->captureWaitForTaskCountWithKmdNotifyInputParams = true;
        csr->waitForTaskCountWithKmdNotifyFallbackReturnValue = NEO::WaitStatus::ready;
        csr->callBaseWaitForCompletionWithTimeout = false;
        csr->returnWaitForCompletionWithTimeout = NEO::WaitStatus::ready;
        return csr;
    }
};

HWTEST_F(CommandListThrottleHostSynchronizeTests, givenImmediateCommandListWithLowThrottleWhenHostSynchronizeWithInfiniteTimeoutThenKmdWaitWithLowThrottleIsUsed) {
    auto commandList = createImmediateCmdList(NEO::QueueThrottle::LOW);
    ASSERT_NE(nullptr, commandList);
    auto csr = prepareCsrForWait<FamilyType>(commandList.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));

    ASSERT_EQ(1u, csr->waitForTaskCountWithKmdNotifyInputParams.size());
    EXPECT_EQ(1u, csr->waitForTaskCountWithKmdNotifyInputParams[0].taskCountToWait);
    EXPECT_EQ(NEO::QueueThrottle::LOW, csr->waitForTaskCountWithKmdNotifyInputParams[0].throttle);
}

HWTEST_F(CommandListThrottleHostSynchronizeTests, givenImmediateCommandListWithMediumThrottleWhenHostSynchronizeWithInfiniteTimeoutThenKmdWaitIsNotUsed) {
    auto commandList = createImmediateCmdList(NEO::QueueThrottle::MEDIUM);
    ASSERT_NE(nullptr, commandList);
    auto csr = prepareCsrForWait<FamilyType>(commandList.get());

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->hostSynchronize(std::numeric_limits<uint64_t>::max()));

    EXPECT_EQ(0u, csr->waitForTaskCountWithKmdNotifyInputParams.size());
    EXPECT_TRUE(csr->latestWaitForCompletionWithTimeoutWaitParams.indefinitelyPoll);
}

} // namespace ult
} // namespace L0
