/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/debugger/active_debugger_fixture.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"
#include <level_zero/ze_api.h>

namespace L0 {
namespace ult {

using CommandQueueDebugCommandsForSldXeHP = Test<ActiveDebuggerFixture>;
using CommandQueueDebugCommandsDebuggerL0XeHP = Test<L0DebuggerHwFixture>;

XEHPTEST_F(CommandQueueDebugCommandsForSldXeHP, givenSteppingA0OrBWhenGlobalSipIsUsedThenMmioIsRestoredAtTheEndOfCmdBuffer) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    std::array<uint32_t, 2> revisions = {hwInfoConfig.getHwRevIdFromStepping(REVID::REVISION_A0, hwInfo),
                                         hwInfoConfig.getHwRevIdFromStepping(REVID::REVISION_B, hwInfo)};

    for (auto revision : revisions) {
        hwInfo.platform.usRevId = revision;

        auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, deviceL0, device->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
        ASSERT_NE(nullptr, commandQueue->commandStream);

        ze_command_list_handle_t commandLists[] = {
            CommandList::create(productFamily, deviceL0, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};

        uint32_t globalSipFound = 0;
        uint32_t debugModeFound = 0;
        uint32_t tdCtlFound = 0;
        std::vector<MI_LOAD_REGISTER_IMM *> globalSip;

        for (uint32_t execCount = 0; execCount < 2; execCount++) {
            auto startPointer = ptrOffset(commandQueue->commandStream->getCpuBase(), commandQueue->commandStream->getUsed());
            auto usedSpaceBefore = commandQueue->commandStream->getUsed();

            auto result = commandQueue->executeCommandLists(1, commandLists, nullptr, true);
            ASSERT_EQ(ZE_RESULT_SUCCESS, result);
            commandQueue->synchronize(0);

            auto usedSpaceAfter = commandQueue->commandStream->getUsed();
            EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

            GenCmdList cmdList;
            ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, startPointer, usedSpaceAfter - usedSpaceBefore));

            auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

            for (size_t i = 0; i < miLoadImm.size(); i++) {
                MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
                ASSERT_NE(nullptr, miLoad);

                if (miLoad->getRegisterOffset() == GlobalSipRegister<FamilyType>::registerOffset) {
                    globalSip.push_back(miLoad);
                    globalSipFound++;
                } else if (miLoad->getRegisterOffset() == 0x20d8u) {
                    debugModeFound++;
                } else if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset<FamilyType>::registerOffset) {
                    tdCtlFound++;
                }
            }
        }

        EXPECT_EQ(1u, debugModeFound);
        EXPECT_EQ(1u, tdCtlFound);

        ASSERT_EQ(4u, globalSipFound);

        auto sipAddress = globalSip[0]->getDataDword();
        auto sipAllocation = SipKernel::getSipKernel(*device).getSipAllocation();
        EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress & 0xfffffff8);

        auto sipAddress2 = globalSip[1]->getDataDword();
        EXPECT_EQ(0u, sipAddress2);

        auto sipAddress3 = globalSip[2]->getDataDword();
        EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress3 & 0xfffffff8);

        auto sipAddress4 = globalSip[3]->getDataDword();
        EXPECT_EQ(0u, sipAddress4);

        auto commandList = CommandList::fromHandle(commandLists[0]);
        commandList->destroy();

        commandQueue->destroy();
    }
}

XEHPTEST_F(CommandQueueDebugCommandsDebuggerL0XeHP, givenSteppingA0OrBWhenGlobalSipIsUsedThenMmioIsRestoredAtTheEndOfCmdBuffer) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    std::array<uint32_t, 2> revisions = {hwInfoConfig.getHwRevIdFromStepping(REVID::REVISION_A0, hwInfo),
                                         hwInfoConfig.getHwRevIdFromStepping(REVID::REVISION_B, hwInfo)};

    for (auto revision : revisions) {
        hwInfo.platform.usRevId = revision;

        auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
        ASSERT_NE(nullptr, commandQueue->commandStream);

        ze_command_list_handle_t commandLists[] = {
            CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};

        uint32_t globalSipFound = 0;
        std::vector<MI_LOAD_REGISTER_IMM *> globalSip;

        for (uint32_t execCount = 0; execCount < 2; execCount++) {
            auto startPointer = ptrOffset(commandQueue->commandStream->getCpuBase(), commandQueue->commandStream->getUsed());
            auto usedSpaceBefore = commandQueue->commandStream->getUsed();

            auto result = commandQueue->executeCommandLists(1, commandLists, nullptr, true);
            ASSERT_EQ(ZE_RESULT_SUCCESS, result);
            commandQueue->synchronize(0);

            auto usedSpaceAfter = commandQueue->commandStream->getUsed();
            EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

            GenCmdList cmdList;
            ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList, startPointer, usedSpaceAfter - usedSpaceBefore));

            auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

            for (size_t i = 0; i < miLoadImm.size(); i++) {
                MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
                ASSERT_NE(nullptr, miLoad);

                if (miLoad->getRegisterOffset() == GlobalSipRegister<FamilyType>::registerOffset) {
                    globalSip.push_back(miLoad);
                    globalSipFound++;
                }
            }
        }

        ASSERT_EQ(4u, globalSipFound);

        auto sipAddress = globalSip[0]->getDataDword();
        auto sipAllocation = SipKernel::getSipKernel(*neoDevice).getSipAllocation();
        EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress & 0xfffffff8);

        auto sipAddress2 = globalSip[1]->getDataDword();
        EXPECT_EQ(0u, sipAddress2);

        auto sipAddress3 = globalSip[2]->getDataDword();
        EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), sipAddress3 & 0xfffffff8);

        auto sipAddress4 = globalSip[3]->getDataDword();
        EXPECT_EQ(0u, sipAddress4);

        auto commandList = CommandList::fromHandle(commandLists[0]);
        commandList->destroy();

        commandQueue->destroy();
    }
}

} // namespace ult
} // namespace L0
