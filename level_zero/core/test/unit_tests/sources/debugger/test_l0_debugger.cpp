/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gen_common/reg_configs/reg_configs_common.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"

#include "test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

using L0DebuggerTest = Test<MockL0DebuggerFixture>;

TEST_F(L0DebuggerTest, givenL0DebuggerWhenCallingIsLegacyThenFalseIsReturned) {
    EXPECT_FALSE(neoDevice->getDebugger()->isLegacy());
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingSourceLevelDebuggerThenNullptrReturned) {
    EXPECT_EQ(nullptr, neoDevice->getSourceLevelDebugger());
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenCallingIsDebuggerActiveThenTrueIsReturned) {
    EXPECT_TRUE(neoDevice->getDebugger()->isDebuggerActive());
}

HWTEST_F(L0DebuggerTest, givenDebuggingEnabledWhenCommandListIsExecutedThenKernelDebugCommandsAreAdded) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    ze_command_queue_desc_t queueDesc = {};
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, false)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_LE(2u, miLoadImm.size());

    size_t debugModeRegisterCount = 0;
    size_t tdDebugControlRegisterCount = 0;

    for (size_t i = 0; i < miLoadImm.size(); i++) {
        MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
        ASSERT_NE(nullptr, miLoad);

        if (miLoad->getRegisterOffset() == DebugModeRegisterOffset<FamilyType>::registerOffset) {
            EXPECT_EQ(DebugModeRegisterOffset<FamilyType>::debugEnabledValue, miLoad->getDataDword());
            debugModeRegisterCount++;
        }
        if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset::registerOffset) {
            EXPECT_EQ(TdDebugControlRegisterOffset::debugEnabledValue, miLoad->getDataDword());
            tdDebugControlRegisterCount++;
        }
    }
    EXPECT_EQ(1u, debugModeRegisterCount);
    EXPECT_EQ(1u, tdDebugControlRegisterCount);

    auto stateSipCmds = findAll<STATE_SIP *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, stateSipCmds.size());

    STATE_SIP *stateSip = genCmdCast<STATE_SIP *>(*stateSipCmds[0]);

    auto systemRoutine = SipKernel::getSipKernelAllocation(*neoDevice);
    ASSERT_NE(nullptr, systemRoutine);
    EXPECT_EQ(systemRoutine->getGpuAddress(), stateSip->getSystemInstructionPointer());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

HWTEST_F(L0DebuggerTest, givenDebuggingEnabledWhenCommandListIsExecutedTwiceThenKernelDebugCommandsAreAddedOnlyOnce) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    ze_command_queue_desc_t queueDesc = {};
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, false)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    size_t debugModeRegisterCount = 0;
    size_t tdDebugControlRegisterCount = 0;

    {

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

        auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

        for (size_t i = 0; i < miLoadImm.size(); i++) {
            MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
            ASSERT_NE(nullptr, miLoad);

            if (miLoad->getRegisterOffset() == DebugModeRegisterOffset<FamilyType>::registerOffset) {
                EXPECT_EQ(DebugModeRegisterOffset<FamilyType>::debugEnabledValue, miLoad->getDataDword());
                debugModeRegisterCount++;
            }
            if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset::registerOffset) {
                EXPECT_EQ(TdDebugControlRegisterOffset::debugEnabledValue, miLoad->getDataDword());
                tdDebugControlRegisterCount++;
            }
        }
        EXPECT_EQ(1u, debugModeRegisterCount);
        EXPECT_EQ(1u, tdDebugControlRegisterCount);
    }
    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter2 = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter2, usedSpaceAfter);

    {
        GenCmdList cmdList2;
        ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
            cmdList2, ptrOffset(commandQueue->commandStream->getCpuBase(), usedSpaceAfter), usedSpaceAfter2 - usedSpaceAfter));

        auto miLoadImm2 = findAll<MI_LOAD_REGISTER_IMM *>(cmdList2.begin(), cmdList2.end());

        for (size_t i = 0; i < miLoadImm2.size(); i++) {
            MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm2[i]);
            ASSERT_NE(nullptr, miLoad);

            if (miLoad->getRegisterOffset() == DebugModeRegisterOffset<FamilyType>::registerOffset) {
                debugModeRegisterCount++;
            }
            if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset::registerOffset) {
                tdDebugControlRegisterCount++;
            }
        }

        EXPECT_EQ(1u, debugModeRegisterCount);
        EXPECT_EQ(1u, tdDebugControlRegisterCount);
    }

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
