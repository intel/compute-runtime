/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_fence.h"

namespace L0 {
namespace ult {

using CommandQueueExecuteCommandListsSimpleTest = Test<DeviceFixture>;

HWTEST2_F(CommandQueueExecuteCommandListsSimpleTest, GivenSynchronousModeWhenExecutingCommandListThenSynchronizeIsCalled, IsAtLeastSkl) {
    ze_command_queue_desc_t desc;
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    auto mockCmdQ = new MockCommandQueueHw<gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQ->initialize(false, false);
    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    mockCmdQ->executeCommandLists(1, commandLists, nullptr, true);
    EXPECT_EQ(mockCmdQ->synchronizedCalled, 1u);
    CommandList::fromHandle(commandLists[0])->destroy();
    mockCmdQ->destroy();
}

HWTEST2_F(CommandQueueExecuteCommandListsSimpleTest, GivenSynchronousModeAndDeviceLostSynchronizeWhenExecutingCommandListThenSynchronizeIsCalledAndDeviceLostIsReturned, IsAtLeastSkl) {
    ze_command_queue_desc_t desc;
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    auto mockCmdQ = new MockCommandQueueHw<gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQ->initialize(false, false);
    mockCmdQ->synchronizeReturnValue = ZE_RESULT_ERROR_DEVICE_LOST;

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};

    const auto result = mockCmdQ->executeCommandLists(1, commandLists, nullptr, true);
    EXPECT_EQ(mockCmdQ->synchronizedCalled, 1u);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);

    CommandList::fromHandle(commandLists[0])->destroy();
    mockCmdQ->destroy();
}

HWTEST2_F(CommandQueueExecuteCommandListsSimpleTest, GivenAsynchronousModeWhenExecutingCommandListThenSynchronizeIsNotCalled, IsAtLeastSkl) {
    ze_command_queue_desc_t desc;
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    auto mockCmdQ = new MockCommandQueueHw<gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQ->initialize(false, false);
    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    mockCmdQ->executeCommandLists(1, commandLists, nullptr, true);
    EXPECT_EQ(mockCmdQ->synchronizedCalled, 0u);
    CommandList::fromHandle(commandLists[0])->destroy();
    mockCmdQ->destroy();
}

HWTEST2_F(CommandQueueExecuteCommandListsSimpleTest, whenUsingFenceThenLastPipeControlUpdatesFenceAllocation, IsAtLeastSkl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_fence_desc_t fenceDesc = {};

    auto fence = whiteboxCast(Fence::create(commandQueue, &fenceDesc));
    ze_fence_handle_t fenceHandle = fence->toHandle();

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle(),
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, fenceHandle, true);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    size_t pipeControlsPostSyncNumber = 0u;
    for (size_t i = 0; i < pipeControls.size(); i++) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControls[i]);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(commandQueue->getCsr()->getTagAllocation()->getGpuAddress(), NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
            EXPECT_EQ(fence->taskCount, pipeControl->getImmediateData());
            pipeControlsPostSyncNumber++;
        }
    }
    EXPECT_EQ(1u, pipeControlsPostSyncNumber);

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    fence->destroy();
    commandQueue->destroy();
}

HWTEST2_F(CommandQueueExecuteCommandListsSimpleTest, givenTwoCommandQueuesUsingSingleCsrWhenExecutingFirstTimeOnBothThenPipelineSelectProgrammedOnce, IsAtMostXeHpcCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    bool additionalPipelineSelect = NEO::HwInfoConfig::get(device->getHwInfo().platform.eProductFamily)->is3DPipelineSelectWARequired() &&
                                    neoDevice->getDefaultEngine().commandStreamReceiver->isRcs();

    ze_result_t returnValue;

    ze_command_list_handle_t commandList = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();
    returnValue = commandQueue->executeCommandLists(1, &commandList, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandQueue->commandStream->getCpuBase(), usedSpaceBefore),
        usedSpaceAfter - usedSpaceBefore));

    auto pipelineSelect = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    size_t expectedFirstPipelineSelectCount = 1u;
    if (additionalPipelineSelect) {
        expectedFirstPipelineSelectCount += 2;
    }
    EXPECT_EQ(expectedFirstPipelineSelectCount, pipelineSelect.size());

    cmdList.clear();

    auto commandQueue2 = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandQueue2->commandStream);

    usedSpaceBefore = commandQueue2->commandStream->getUsed();
    returnValue = commandQueue2->executeCommandLists(1, &commandList, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    usedSpaceAfter = commandQueue2->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandQueue2->commandStream->getCpuBase(), usedSpaceBefore),
        usedSpaceAfter - usedSpaceBefore));

    pipelineSelect = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    constexpr size_t expectedSecondPipelineSelectCount = 0u;
    EXPECT_EQ(expectedSecondPipelineSelectCount, pipelineSelect.size());

    CommandList::fromHandle(commandList)->destroy();
    commandQueue->destroy();
    commandQueue2->destroy();
}

using IsMmioPreemptionUsed = IsWithinGfxCore<IGFX_GEN9_CORE, IGFX_XE_HPC_CORE>;

HWTEST2_F(CommandQueueExecuteCommandListsSimpleTest, givenTwoCommandQueuesUsingSingleCsrWhenExecutingFirstTimeOnBothQueuesThenPreemptionModeIsProgrammedOnce, IsMmioPreemptionUsed) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    ze_result_t returnValue;

    ze_command_list_handle_t commandList = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    CommandList::fromHandle(commandList)->commandListPreemptionMode = NEO::PreemptionMode::ThreadGroup;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();
    returnValue = commandQueue->executeCommandLists(1, &commandList, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandQueue->commandStream->getCpuBase(), usedSpaceBefore),
        usedSpaceAfter - usedSpaceBefore));

    auto loadRegisterImmList = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    size_t foundPreemptionMmioCount = 0;
    for (auto it : loadRegisterImmList) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (cmd->getRegisterOffset() == 0x2580) {
            foundPreemptionMmioCount++;
        }
    }

    constexpr size_t expectedFirstPreemptionMmioCount = 1u;
    EXPECT_EQ(expectedFirstPreemptionMmioCount, foundPreemptionMmioCount);

    cmdList.clear();
    foundPreemptionMmioCount = 0;

    auto commandQueue2 = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandQueue2->commandStream);

    usedSpaceBefore = commandQueue2->commandStream->getUsed();
    returnValue = commandQueue2->executeCommandLists(1, &commandList, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    usedSpaceAfter = commandQueue2->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList,
        ptrOffset(commandQueue2->commandStream->getCpuBase(), usedSpaceBefore),
        usedSpaceAfter - usedSpaceBefore));

    loadRegisterImmList = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    for (auto it : loadRegisterImmList) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*it);
        if (cmd->getRegisterOffset() == 0x2580) {
            foundPreemptionMmioCount++;
        }
    }

    constexpr size_t expectedSecondPreemptionMmioCount = 0u;
    EXPECT_EQ(expectedSecondPreemptionMmioCount, foundPreemptionMmioCount);

    CommandList::fromHandle(commandList)->destroy();
    commandQueue->destroy();
    commandQueue2->destroy();
}

} // namespace ult
} // namespace L0
