/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include <level_zero/ze_api.h>

#include "active_debugger_fixture.h"
#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using CommandQueueDebugCommandsTest = Test<ActiveDebuggerFixture>;

HWTEST_F(CommandQueueDebugCommandsTest, givenDebuggingEnabledWhenCommandListIsExecutedThenKernelDebugCommandsAreAdded) {
    ze_command_queue_desc_t queueDesc = {};
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, deviceL0, device->getDefaultEngine().commandStreamReceiver, &queueDesc, false));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, deviceL0, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_LE(2u, miLoadImm.size());

    MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[0]);
    ASSERT_NE(nullptr, miLoad);

    EXPECT_EQ(DebugModeRegisterOffset<FamilyType>::registerOffset, miLoad->getRegisterOffset());
    EXPECT_EQ(DebugModeRegisterOffset<FamilyType>::debugEnabledValue, miLoad->getDataDword());

    miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[1]);
    ASSERT_NE(nullptr, miLoad);

    EXPECT_EQ(TdDebugControlRegisterOffset<FamilyType>::registerOffset, miLoad->getRegisterOffset());
    EXPECT_EQ(TdDebugControlRegisterOffset<FamilyType>::debugEnabledValue, miLoad->getDataDword());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

HWTEST_F(CommandQueueDebugCommandsTest, givenDebuggingEnabledWhenCommandListIsExecutedTwiceThenKernelDebugCommandsAreAddedOnlyOnce) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    ze_command_queue_desc_t queueDesc = {};
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, deviceL0, device->getDefaultEngine().commandStreamReceiver, &queueDesc, false));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, deviceL0, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle()};
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
            if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset<FamilyType>::registerOffset) {
                EXPECT_EQ(TdDebugControlRegisterOffset<FamilyType>::debugEnabledValue, miLoad->getDataDword());
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
            if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset<FamilyType>::registerOffset) {
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

using DeviceWithDebuggerEnabledTest = Test<ActiveDebuggerFixture>;

TEST_F(DeviceWithDebuggerEnabledTest, givenDebuggingEnabledWhenDeviceIsCreatedThenItHasDebugSurfaceCreatedWithCorrectAllocationType) {
    ASSERT_NE(nullptr, deviceL0->getDebugSurface());
    EXPECT_EQ(NEO::GraphicsAllocation::AllocationType::DEBUG_CONTEXT_SAVE_AREA, deviceL0->getDebugSurface()->getAllocationType());
}

TEST_F(DeviceWithDebuggerEnabledTest, givenSldDebuggerWhenGettingL0DebuggerThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, deviceL0->getL0Debugger());
}

struct TwoSubDevicesDebuggerEnabledTest : public ActiveDebuggerFixture, public ::testing::Test {
    void SetUp() override { // NOLINT(readability-identifier-naming)
        DebugManager.flags.CreateMultipleSubDevices.set(2);
        VariableBackup<bool> mockDeviceFlagBackup(&MockDevice::createSingleDevice, false);
        ActiveDebuggerFixture::SetUp();
    }
    void TearDown() override { // NOLINT(readability-identifier-naming)
        ActiveDebuggerFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

TEST_F(TwoSubDevicesDebuggerEnabledTest, givenDebuggingEnabledWhenSubDevicesAreCreatedThenDebugSurfaceFromRootDeviceIsSet) {
    auto subDevice0 = static_cast<L0::DeviceImp *>(deviceL0)->subDevices[0];
    auto subDevice1 = static_cast<L0::DeviceImp *>(deviceL0)->subDevices[1];

    EXPECT_NE(nullptr, subDevice0->getDebugSurface());
    EXPECT_NE(nullptr, subDevice1->getDebugSurface());

    EXPECT_EQ(subDevice0->getDebugSurface(), subDevice1->getDebugSurface());
    EXPECT_EQ(deviceL0->getDebugSurface(), subDevice0->getDebugSurface());
}

} // namespace ult
} // namespace L0
