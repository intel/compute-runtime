/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/utilities/software_tags_manager.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"

#include "test.h"

#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

struct CommandQueueExecuteCommandLists : public Test<DeviceFixture> {
    void SetUp() override {
        DeviceFixture::SetUp();

        ze_result_t returnValue;
        commandLists[0] = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle();
        ASSERT_NE(nullptr, commandLists[0]);
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

        commandLists[1] = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle();
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

    template <typename FamilyType>
    void twoCommandListCommandPreemptionTest(bool preemptionCmdProgramming);

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

            auto sipAllocation = SipKernel::getSipKernel(*neoDevice).getSipAllocation();
            STATE_SIP *stateSipCmd = reinterpret_cast<STATE_SIP *>(*itorSip);
            EXPECT_EQ(sipAllocation->getGpuAddressToPatch(), stateSipCmd->getSystemInstructionPointer());
        } else {
            EXPECT_EQ(cmdList.end(), itorSip);
        }
        commandQueue->destroy();
    }
}

using IsSklOrAbove = IsAtLeastProduct<IGFX_SKYLAKE>;
HWTEST2_F(CommandQueueExecuteCommandLists, givenMidThreadPreemptionWhenCommandsAreExecutedTwoTimesThenStateSipIsAddedOnlyTheFirstTime, IsSklOrAbove) {
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

            auto sipAllocation = SipKernel::getSipKernel(*neoDevice).getSipAllocation();
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

        // No preemption reprogramming
        auto secondExecMmioCount = countMmio<FamilyType>(cmdList2.begin(), cmdList2.end(), 0x2580u);
        EXPECT_EQ(0u, secondExecMmioCount);

        commandQueue->destroy();
    }
}

HWTEST2_F(CommandQueueExecuteCommandLists, givenCommandListsWithCooperativeAndNonCooperativeKernelsWhenExecuteCommandListsIsCalledThenErrorIsReturned, IsSklOrAbove) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);

    auto pCommandQueue = new MockCommandQueueHw<gfxCoreFamily>{device, csr, &desc};
    pCommandQueue->initialize(false, false);

    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    ze_group_count_t threadGroupDimensions{1, 1, 1};
    auto pCommandListWithCooperativeKernels = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandListWithCooperativeKernels->initialize(device, NEO::EngineGroupType::Compute, 0u);
    pCommandListWithCooperativeKernels->appendLaunchKernelWithParams(&kernel, &threadGroupDimensions, nullptr, false, false, true);

    auto pCommandListWithNonCooperativeKernels = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandListWithNonCooperativeKernels->initialize(device, NEO::EngineGroupType::Compute, 0u);
    pCommandListWithNonCooperativeKernels->appendLaunchKernelWithParams(&kernel, &threadGroupDimensions, nullptr, false, false, false);

    {
        ze_command_list_handle_t commandLists[] = {pCommandListWithCooperativeKernels->toHandle(),
                                                   pCommandListWithNonCooperativeKernels->toHandle()};
        auto result = pCommandQueue->executeCommandLists(2, commandLists, nullptr, false);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE, result);
    }
    {
        ze_command_list_handle_t commandLists[] = {pCommandListWithNonCooperativeKernels->toHandle(),
                                                   pCommandListWithCooperativeKernels->toHandle()};
        auto result = pCommandQueue->executeCommandLists(2, commandLists, nullptr, false);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE, result);
    }
    pCommandQueue->destroy();
}

template <typename FamilyType>
void CommandQueueExecuteCommandLists::twoCommandListCommandPreemptionTest(bool preemptionCmdProgramming) {
    ze_command_queue_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(
        productFamily,
        device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);
    commandQueue->preemptionCmdSyncProgramming = preemptionCmdProgramming;
    preemptionCmdProgramming = NEO::PreemptionHelper::getRequiredCmdStreamSize<FamilyType>(NEO::PreemptionMode::ThreadGroup, NEO::PreemptionMode::Disabled) > 0u;
    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    auto commandListDisabled = whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    commandListDisabled->commandListPreemptionMode = NEO::PreemptionMode::Disabled;

    auto commandListThreadGroup = whitebox_cast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    commandListThreadGroup->commandListPreemptionMode = NEO::PreemptionMode::ThreadGroup;

    ze_command_list_handle_t commandLists[] = {commandListDisabled->toHandle(),
                                               commandListThreadGroup->toHandle(),
                                               commandListDisabled->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->synchronize(0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::PreemptionMode::Disabled, commandQueue->commandQueuePreemptionMode);

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(NEO::PreemptionMode::Disabled, commandQueue->commandQueuePreemptionMode);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandQueue->commandStream->getCpuBase(), usedSpaceAfter));
    using STATE_SIP = typename FamilyType::STATE_SIP;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    auto preemptionMode = neoDevice->getPreemptionMode();
    GenCmdList::iterator itor = cmdList.begin();

    GenCmdList::iterator itorStateSip = find<STATE_SIP *>(cmdList.begin(), cmdList.end());
    if (preemptionMode == NEO::PreemptionMode::MidThread) {
        EXPECT_NE(itorStateSip, cmdList.end());

        itor = itorStateSip;
    } else {
        EXPECT_EQ(itorStateSip, cmdList.end());
    }

    MI_LOAD_REGISTER_IMM *lriCmd = nullptr;
    auto itorLri = find<MI_LOAD_REGISTER_IMM *>(itor, cmdList.end());
    if (preemptionCmdProgramming) {
        EXPECT_NE(itorLri, cmdList.end());
        //Initial cmdQ preemption
        lriCmd = static_cast<MI_LOAD_REGISTER_IMM *>(*itorLri);
        EXPECT_EQ(0x2580u, lriCmd->getRegisterOffset());

        itor = itorLri;
    } else {
        EXPECT_EQ(itorLri, cmdList.end());
    }

    uint32_t data = 0;
    //next should be BB_START to 1st Disabled preemption Cmd List
    auto itorBBStart = find<MI_BATCH_BUFFER_START *>(itor, cmdList.end());
    EXPECT_NE(itorBBStart, cmdList.end());
    itor = itorBBStart;

    itorLri = find<MI_LOAD_REGISTER_IMM *>(itor, cmdList.end());
    if (preemptionCmdProgramming) {
        EXPECT_NE(itorLri, cmdList.end());

        lriCmd = static_cast<MI_LOAD_REGISTER_IMM *>(*itorLri);
        EXPECT_EQ(0x2580u, lriCmd->getRegisterOffset());
        data = (1 << 1) | (((1 << 1) | (1 << 2)) << 16);
        EXPECT_EQ(data, lriCmd->getDataDword());

        //verify presence of sync PIPE_CONTROL just before LRI switching to thread-group
        if (commandQueue->preemptionCmdSyncProgramming) {
            auto itorPipeControl = find<PIPE_CONTROL *>(itor, itorLri);
            EXPECT_NE(itorPipeControl, cmdList.end());
        }

        itor = itorLri;
    } else {
        EXPECT_EQ(itorLri, cmdList.end());
    }

    //start of thread-group command list
    itorBBStart = find<MI_BATCH_BUFFER_START *>(itor, cmdList.end());
    EXPECT_NE(itorBBStart, cmdList.end());
    itor = itorBBStart;

    itorLri = find<MI_LOAD_REGISTER_IMM *>(itor, cmdList.end());
    if (preemptionCmdProgramming) {
        EXPECT_NE(itorLri, cmdList.end());
        lriCmd = static_cast<MI_LOAD_REGISTER_IMM *>(*itorLri);
        EXPECT_EQ(0x2580u, lriCmd->getRegisterOffset());
        data = (1 << 2) | (((1 << 1) | (1 << 2)) << 16);
        EXPECT_EQ(data, lriCmd->getDataDword());

        //verify presence of sync PIPE_CONTROL just before LRI switching to thread-group
        if (commandQueue->preemptionCmdSyncProgramming) {
            auto itorPipeControl = find<PIPE_CONTROL *>(itor, itorLri);
            EXPECT_NE(itorPipeControl, cmdList.end());
        }

        itor = itorLri;
    } else {
        EXPECT_EQ(itorLri, cmdList.end());
    }

    //start of thread-group command list
    itorBBStart = find<MI_BATCH_BUFFER_START *>(itor, cmdList.end());
    EXPECT_NE(itorBBStart, cmdList.end());
    itor = itorBBStart;

    // BB end
    auto itorBBEnd = find<MI_BATCH_BUFFER_START *>(itor, cmdList.end());
    EXPECT_NE(itorBBStart, cmdList.end());

    auto allStateSips = findAll<STATE_SIP *>(cmdList.begin(), cmdList.end());
    if (preemptionMode == NEO::PreemptionMode::MidThread) {
        EXPECT_EQ(1u, allStateSips.size());
    } else {
        EXPECT_EQ(0u, allStateSips.size());
    }

    auto firstExecMmioCount = countMmio<FamilyType>(cmdList.begin(), itorBBEnd, 0x2580u);
    size_t expectedMmioCount = preemptionCmdProgramming ? 4u : 0u;
    EXPECT_EQ(expectedMmioCount, firstExecMmioCount);

    // Count next MMIOs for preemption - only two should be present as last cmdlist from 1st exec
    // and first cmdlist from 2nd exec has the same mode - cmdQ state should remember it
    auto secondExecMmioCount = countMmio<FamilyType>(itorBBEnd, cmdList.end(), 0x2580u);
    expectedMmioCount = preemptionCmdProgramming ? 2u : 0u;
    EXPECT_EQ(expectedMmioCount, secondExecMmioCount);

    commandListDisabled->destroy();
    commandListThreadGroup->destroy();
    commandQueue->destroy();
}

HWTEST2_F(CommandQueueExecuteCommandLists, GivenCmdListsWithDifferentPreemptionModesWhenExecutingThenQueuePreemptionIsSwitchedAndStateSipProgrammedOnce, IsSklOrAbove) {
    twoCommandListCommandPreemptionTest<FamilyType>(false);
}

HWTEST2_F(CommandQueueExecuteCommandLists, GivenCmdListsWithDifferentPreemptionModesWhenNoCmdStreamPreemptionRequiredThenNoCmdStreamProgrammingAndStateSipProgrammedOnce, IsSklOrAbove) {
    twoCommandListCommandPreemptionTest<FamilyType>(true);
}

struct CommandQueueExecuteCommandListSWTagsTests : public Test<DeviceFixture> {
    void SetUp() override {
        DebugManager.flags.EnableSWTags.set(true);
        DeviceFixture::SetUp();

        ze_result_t returnValue;
        commandLists[0] = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle();
        ASSERT_NE(nullptr, commandLists[0]);
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

        ze_command_queue_desc_t desc = {};
        commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
        ASSERT_NE(nullptr, commandQueue->commandStream);
    }

    void TearDown() override {
        commandQueue->destroy();

        for (auto i = 0u; i < numCommandLists; i++) {
            auto commandList = CommandList::fromHandle(commandLists[i]);
            commandList->destroy();
        }

        DeviceFixture::TearDown();
    }

    DebugManagerStateRestore dbgRestorer;
    const static uint32_t numCommandLists = 1;
    ze_command_list_handle_t commandLists[numCommandLists];
    L0::ult::CommandQueue *commandQueue;
};

HWTEST_F(CommandQueueExecuteCommandListSWTagsTests, givenEnableSWTagsWhenExecutingCommandListThenHeapAddressesAreInserted) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using PARSE = typename FamilyType::PARSE;

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    auto result = commandQueue->executeCommandLists(1, commandLists, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto sdis = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_LE(2u, sdis.size());

    auto dbgdocSDI = genCmdCast<MI_STORE_DATA_IMM *>(*sdis[0]);
    auto dbgddiSDI = genCmdCast<MI_STORE_DATA_IMM *>(*sdis[1]);

    EXPECT_EQ(dbgdocSDI->getAddress(), neoDevice->getRootDeviceEnvironment().tagsManager->getBXMLHeapAllocation()->getGpuAddress());
    EXPECT_EQ(dbgddiSDI->getAddress(), neoDevice->getRootDeviceEnvironment().tagsManager->getSWTagHeapAllocation()->getGpuAddress());
}

HWTEST_F(CommandQueueExecuteCommandListSWTagsTests, givenEnableSWTagsAndCommandListWithDifferentPreemtpionWhenExecutingCommandListThenPipeControlReasonTagIsInserted) {
    using MI_NOOP = typename FamilyType::MI_NOOP;
    using PARSE = typename FamilyType::PARSE;

    whitebox_cast(CommandList::fromHandle(commandLists[0]))->commandListPreemptionMode = PreemptionMode::Disabled;
    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    auto result = commandQueue->executeCommandLists(1, commandLists, nullptr, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto noops = findAll<MI_NOOP *>(cmdList.begin(), cmdList.end());
    ASSERT_LE(2u, noops.size());

    bool tagFound = false;
    for (auto it = noops.begin(); it != noops.end() && !tagFound; ++it) {

        auto noop = genCmdCast<MI_NOOP *>(*(*it));
        if (NEO::SWTags::BaseTag::getMarkerNoopID(SWTags::OpCode::PipeControlReason) == noop->getIdentificationNumber() &&
            noop->getIdentificationNumberRegisterWriteEnable() == true &&
            ++it != noops.end()) {

            noop = genCmdCast<MI_NOOP *>(*(*it));
            if (noop->getIdentificationNumber() & 1 << 21 &&
                noop->getIdentificationNumberRegisterWriteEnable() == false) {
                tagFound = true;
            }
        }
    }
    EXPECT_TRUE(tagFound);
}

} // namespace ult
} // namespace L0
