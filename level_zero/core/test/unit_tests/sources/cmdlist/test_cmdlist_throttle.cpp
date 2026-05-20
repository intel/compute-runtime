/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/queue_throttle.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily,
                                                                              device,
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily,
                                                                              device,
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
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily,
                                                                              device,
                                                                              &desc,
                                                                              false,
                                                                              NEO::EngineGroupType::renderCompute,
                                                                              returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto whiteBoxCmdList = CommandList::whiteboxCast(commandList.get());
    EXPECT_EQ(NEO::QueueThrottle::HIGH, whiteBoxCmdList->queueThrottle);
}

} // namespace ult
} // namespace L0
