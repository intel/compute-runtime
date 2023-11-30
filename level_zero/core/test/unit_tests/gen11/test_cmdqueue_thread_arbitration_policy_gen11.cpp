/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/reg_configs.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

using CommandQueueThreadArbitrationPolicyTests = Test<CommandQueueThreadArbitrationPolicyFixture>;

HWTEST2_F(CommandQueueThreadArbitrationPolicyTests,
          whenCommandListIsExecutedThenDefaultRoundRobinThreadArbitrationPolicyIsUsed,
          IsGen11HP) {
    size_t usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t hCommandList = commandList->toHandle();
    auto result = commandQueue->executeCommandLists(1, &hCommandList, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    EXPECT_GE(2u, miLoadImm.size());

    for (auto it : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
            EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::RoundRobin),
                      cmd->getDataDword());
        }
    }
}

HWTEST2_F(CommandQueueThreadArbitrationPolicyTests,
          whenCommandListIsExecutedAndOverrideThreadArbitrationPolicyDebugFlagIsSetToZeroThenAgeBasedThreadArbitrationPolicyIsUsed,
          IsGen11HP) {
    debugManager.flags.OverrideThreadArbitrationPolicy.set(0);

    size_t usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t hCommandList = commandList->toHandle();
    auto result = commandQueue->executeCommandLists(1, &hCommandList, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    EXPECT_GE(2u, miLoadImm.size());

    for (auto it : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
            EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::AgeBased),
                      cmd->getDataDword());
        }
    }
}

HWTEST2_F(CommandQueueThreadArbitrationPolicyTests,
          whenCommandListIsExecutedAndOverrideThreadArbitrationPolicyDebugFlagIsSetToOneThenRoundRobinThreadArbitrationPolicyIsUsed,
          IsGen11HP) {
    debugManager.flags.OverrideThreadArbitrationPolicy.set(1);

    size_t usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t hCommandList = commandList->toHandle();
    auto result = commandQueue->executeCommandLists(1, &hCommandList, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    EXPECT_GE(2u, miLoadImm.size());

    for (auto it : miLoadImm) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (cmd->getRegisterOffset() == NEO::DebugControlReg2::address) {
            EXPECT_EQ(NEO::DebugControlReg2::getRegData(NEO::ThreadArbitrationPolicy::RoundRobin),
                      cmd->getDataDword());
        }
    }
}

} // namespace ult
} // namespace L0
