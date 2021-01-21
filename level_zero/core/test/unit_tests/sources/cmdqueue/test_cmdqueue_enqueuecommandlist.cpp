/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

#include "test.h"

#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"

namespace L0 {
namespace ult {

struct CommandQueueExecuteCommandLists : public Test<DeviceFixture> {
    void SetUp() override {
        DeviceFixture::SetUp();

        ze_result_t returnValue;
        commandLists[0] = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle();
        ASSERT_NE(nullptr, commandLists[0]);
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

        commandLists[1] = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle();
        ASSERT_NE(nullptr, commandLists[1]);
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    }

    void TearDown() override {
        for (auto i = 0u; i < numCommandLists; i++) {
            auto commandList = CommandList::fromHandle(commandLists[i]);
            commandList->destroy();
        }

        DeviceFixture::TearDown();
    }

    const static uint32_t numCommandLists = 2;
    ze_command_list_handle_t commandLists[numCommandLists];
};

HWTEST_F(CommandQueueExecuteCommandLists, whenASecondLevelBatchBufferPerCommandListAddedThenProperSizeExpected) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using PARSE = typename FamilyType::PARSE;

    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList,
                                          ptrOffset(commandQueue->commandStream->getCpuBase(), 0),
                                          usedSpaceAfter));

    auto itorCurrent = cmdList.begin();
    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        auto allocation = commandList->commandContainer.getCmdBufferAllocations()[0];

        itorCurrent = find<MI_BATCH_BUFFER_START *>(itorCurrent, cmdList.end());
        ASSERT_NE(cmdList.end(), itorCurrent);

        auto bbs = genCmdCast<MI_BATCH_BUFFER_START *>(*itorCurrent++);
        ASSERT_NE(nullptr, bbs);
        EXPECT_EQ(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH,
                  bbs->getSecondLevelBatchBuffer());
        EXPECT_EQ(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT,
                  bbs->getAddressSpaceIndicator());
        EXPECT_EQ(allocation->getGpuAddress(), bbs->getBatchBufferStartAddressGraphicsaddress472());
    }

    auto itorBBE = find<MI_BATCH_BUFFER_END *>(itorCurrent, cmdList.end());
    EXPECT_NE(cmdList.end(), itorBBE);

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueExecuteCommandLists, whenUsingFenceThenExpectEndingPipeControlUpdatingFenceAllocation, IsGen9) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;
    using PARSE = typename FamilyType::PARSE;

    ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_fence_desc_t fenceDesc{};
    auto fence = whitebox_cast(Fence::create(commandQueue, &fenceDesc));
    ASSERT_NE(nullptr, fence);

    ze_fence_handle_t fenceHandle = fence->toHandle();

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, fenceHandle, true);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList,
                                          ptrOffset(commandQueue->commandStream->getCpuBase(), 0),
                                          usedSpaceAfter));

    //on some platforms Fence update requires more than single PIPE_CONTROL, Fence tag update should be in the third to last command in SKL
    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    //we require at least one PIPE_CONTROL
    ASSERT_LE(1u, pipeControls.size());
    PIPE_CONTROL *fenceUpdate = genCmdCast<PIPE_CONTROL *>(*pipeControls[pipeControls.size() - 3]);

    uint64_t low = fenceUpdate->getAddress();
    uint64_t high = fenceUpdate->getAddressHigh();
    uint64_t fenceGpuAddress = (high << 32) | low;
    EXPECT_EQ(fence->getGpuAddress(), fenceGpuAddress);

    EXPECT_EQ(POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, fenceUpdate->getPostSyncOperation());

    uint64_t fenceImmData = Fence::STATE_SIGNALED;
    EXPECT_EQ(fenceImmData, fenceUpdate->getImmediateData());

    fence->destroy();
    commandQueue->destroy();
}

HWTEST_F(CommandQueueExecuteCommandLists, whenExecutingCommandListsThenEndingPipeControlCommandIsExpected) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;
    using PARSE = typename FamilyType::PARSE;

    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList,
                                          ptrOffset(commandQueue->commandStream->getCpuBase(), 0),
                                          usedSpaceAfter));

    // Pipe control w/ Post-sync operation should be the last command
    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    // We require at least one PIPE_CONTROL
    ASSERT_LE(1u, pipeControls.size());
    PIPE_CONTROL *taskCountToWriteCmd = genCmdCast<PIPE_CONTROL *>(*pipeControls[pipeControls.size() - 1]);

    EXPECT_EQ(POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, taskCountToWriteCmd->getPostSyncOperation());

    uint64_t taskCountToWrite = neoDevice->getDefaultEngine().commandStreamReceiver->peekTaskCount();
    EXPECT_EQ(taskCountToWrite, taskCountToWriteCmd->getImmediateData());

    commandQueue->destroy();
}

using CommandQueueExecuteSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
HWTEST2_F(CommandQueueExecuteCommandLists, givenCommandQueueHaving2CommandListsThenMVSIsProgrammedWithMaxPTSS, CommandQueueExecuteSupport) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PARSE = typename FamilyType::PARSE;
    ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));

    CommandList::fromHandle(commandLists[0])->setCommandListPerThreadScratchSize(512u);
    CommandList::fromHandle(commandLists[1])->setCommandListPerThreadScratchSize(1024u);

    ASSERT_NE(nullptr, commandQueue->commandStream);
    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1024u, neoDevice->getDefaultEngine().commandStreamReceiver->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList,
                                          ptrOffset(commandQueue->commandStream->getCpuBase(), 0),
                                          usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto GSBAStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have only 1 state added
    ASSERT_EQ(1u, mediaVfeStates.size());
    ASSERT_EQ(1u, GSBAStates.size());

    CommandList::fromHandle(commandLists[0])->reset();
    CommandList::fromHandle(commandLists[1])->reset();
    CommandList::fromHandle(commandLists[0])->setCommandListPerThreadScratchSize(2048u);
    CommandList::fromHandle(commandLists[1])->setCommandListPerThreadScratchSize(1024u);

    ASSERT_NE(nullptr, commandQueue->commandStream);
    usedSpaceBefore = commandQueue->commandStream->getUsed();

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2048u, neoDevice->getDefaultEngine().commandStreamReceiver->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList1;
    ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList1,
                                          ptrOffset(commandQueue->commandStream->getCpuBase(), 0),
                                          usedSpaceAfter));

    mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList1.begin(), cmdList1.end());
    GSBAStates = findAll<STATE_BASE_ADDRESS *>(cmdList1.begin(), cmdList1.end());
    // We should have 2 states added
    ASSERT_EQ(2u, mediaVfeStates.size());
    ASSERT_EQ(2u, GSBAStates.size());

    commandQueue->destroy();
}

HWTEST_F(CommandQueueExecuteCommandLists, givenMidThreadPreemptionWhenCommandsAreExecutedThenStateSipIsAdded) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    using PARSE = typename FamilyType::PARSE;

    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    std::array<bool, 2> testedInternalFlags = {true, false};

    for (auto flagInternal : testedInternalFlags) {
        ze_result_t returnValue;
        auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                               device,
                                                               neoDevice->getDefaultEngine().commandStreamReceiver,
                                                               &desc,
                                                               false,
                                                               flagInternal,
                                                               returnValue));
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

        ASSERT_NE(nullptr, commandQueue->commandStream);

        auto usedSpaceBefore = commandQueue->commandStream->getUsed();

        auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto usedSpaceAfter = commandQueue->commandStream->getUsed();
        ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

        GenCmdList cmdList;
        ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

        auto itorSip = find<STATE_SIP *>(cmdList.begin(), cmdList.end());

        auto preemptionMode = neoDevice->getPreemptionMode();
        if (preemptionMode == NEO::PreemptionMode::MidThread) {
            EXPECT_NE(cmdList.end(), itorSip);

            auto sipAllocation = SipKernel::getSipKernelAllocation(*neoDevice);
            STATE_SIP *stateSipCmd = reinterpret_cast<STATE_SIP *>(*itorSip);
            EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), stateSipCmd->getSystemInstructionPointer());
        } else {
            EXPECT_EQ(cmdList.end(), itorSip);
        }
        commandQueue->destroy();
    }
}

HWTEST_F(CommandQueueExecuteCommandLists, givenMidThreadPreemptionWhenCommandsAreExecutedTwoTimesThenStateSipIsAddedOnlyTheFirstTime) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    using PARSE = typename FamilyType::PARSE;

    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    std::array<bool, 2> testedInternalFlags = {true, false};

    for (auto flagInternal : testedInternalFlags) {
        ze_result_t returnValue;
        auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                               device,
                                                               neoDevice->getDefaultEngine().commandStreamReceiver,
                                                               &desc,
                                                               false,
                                                               flagInternal,
                                                               returnValue));
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

        ASSERT_NE(nullptr, commandQueue->commandStream);

        auto usedSpaceBefore = commandQueue->commandStream->getUsed();

        auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        result = commandQueue->synchronize(0);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto usedSpaceAfter = commandQueue->commandStream->getUsed();
        ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

        GenCmdList cmdList;
        ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

        auto itorSip = find<STATE_SIP *>(cmdList.begin(), cmdList.end());

        auto preemptionMode = neoDevice->getPreemptionMode();
        if (preemptionMode == NEO::PreemptionMode::MidThread) {
            EXPECT_NE(cmdList.end(), itorSip);

            auto sipAllocation = SipKernel::getSipKernelAllocation(*neoDevice);
            STATE_SIP *stateSipCmd = reinterpret_cast<STATE_SIP *>(*itorSip);
            EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), stateSipCmd->getSystemInstructionPointer());
        } else {
            EXPECT_EQ(cmdList.end(), itorSip);
        }

        result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        result = commandQueue->synchronize(0);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto usedSpaceAfterSecondExec = commandQueue->commandStream->getUsed();
        GenCmdList cmdList2;
        ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList2, ptrOffset(commandQueue->commandStream->getCpuBase(), usedSpaceAfter), usedSpaceAfterSecondExec));

        itorSip = find<STATE_SIP *>(cmdList2.begin(), cmdList2.end());
        EXPECT_EQ(cmdList2.end(), itorSip);

        commandQueue->destroy();
    }
}

} // namespace ult
} // namespace L0
