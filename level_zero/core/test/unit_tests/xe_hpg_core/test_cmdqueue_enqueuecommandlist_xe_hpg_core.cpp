/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include <level_zero/ze_api.h>

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

using CommandQueueExecuteCommandListsXeHpgCore = Test<DeviceFixture>;

XE_HPG_CORETEST_F(CommandQueueExecuteCommandListsXeHpgCore, WhenExecutingCmdListsThenPipelineSelectAndCfeStateAreAddedToCmdBuffer) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(
        productFamily,
        device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);
    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    CommandList::fromHandle(commandLists[0])->close();

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    using CFE_STATE = typename FamilyType::CFE_STATE;
    auto itorCFE = find<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(itorCFE, cmdList.end());

    // Should have a PS before a CFE
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    auto itorPS = find<PIPELINE_SELECT *>(cmdList.begin(), itorCFE);
    ASSERT_NE(itorPS, itorCFE);
    {
        auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorPS);
        EXPECT_EQ(cmd->getMaskBits() & 3u, 3u);
        EXPECT_EQ(cmd->getPipelineSelection(), PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    }

    CommandList::fromHandle(commandLists[0])->destroy();
    commandQueue->destroy();
}

XE_HPG_CORETEST_F(CommandQueueExecuteCommandListsXeHpgCore, WhenExecutingCmdListsThenStateBaseAddressForGeneralStateBaseAddressIsNotAdded) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableStateBaseAddressTracking.set(0);

    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(
        productFamily,
        device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);
    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    CommandList::fromHandle(commandLists[0])->close();

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto itorSba = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(itorSba, cmdList.end());

    CommandList::fromHandle(commandLists[0])->destroy();
    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
