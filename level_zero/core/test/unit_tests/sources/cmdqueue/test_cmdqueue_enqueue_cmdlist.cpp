/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/utilities/software_tags_manager.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_fence.h"
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

struct MultiDeviceCommandQueueExecuteCommandLists : public Test<MultiDeviceFixture> {
    void SetUp() override {
        DebugManager.flags.EnableWalkerPartition.set(1);
        numRootDevices = 1u;
        MultiDeviceFixture::SetUp();

        uint32_t deviceCount = 1;
        ze_device_handle_t deviceHandle;
        driverHandle->getDevice(&deviceCount, &deviceHandle);
        device = Device::fromHandle(deviceHandle);
        ASSERT_NE(nullptr, device);

        ze_result_t returnValue;
        commandLists[0] = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle();
        ASSERT_NE(nullptr, commandLists[0]);
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
        EXPECT_EQ(2u, CommandList::fromHandle(commandLists[0])->partitionCount);

        commandLists[1] = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle();
        ASSERT_NE(nullptr, commandLists[1]);
        EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
        EXPECT_EQ(2u, CommandList::fromHandle(commandLists[1])->partitionCount);
    }

    void TearDown() override {
        for (auto i = 0u; i < numCommandLists; i++) {
            auto commandList = CommandList::fromHandle(commandLists[i]);
            commandList->destroy();
        }

        MultiDeviceFixture::TearDown();
    }

    L0::Device *device = nullptr;
    const static uint32_t numCommandLists = 2;
    ze_command_list_handle_t commandLists[numCommandLists];
};

HWTEST_F(CommandQueueExecuteCommandLists, whenACommandListExecutedRequiresUncachedMOCSThenSuccessisReturned) {
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

    auto commandList1 = whitebox_cast(CommandList::fromHandle(commandLists[0]));
    auto commandList2 = whitebox_cast(CommandList::fromHandle(commandLists[1]));
    commandList1->requiresQueueUncachedMocs = true;
    commandList2->requiresQueueUncachedMocs = true;
    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueExecuteCommandLists, givenCommandListThatRequiresDisabledEUFusionWhenExecutingCommandListsThenCommandQueueHasProperStreamProperties) {
    struct WhiteBoxCommandList : public L0::CommandList {
        using CommandList::CommandList;
        using CommandList::requiredStreamState;
    };

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

    auto commandList1 = static_cast<WhiteBoxCommandList *>(CommandList::fromHandle(commandLists[0]));
    commandList1->requiredStreamState.frontEndState.disableEUFusion.set(true);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1, commandQueue->getCsr()->getStreamProperties().frontEndState.disableEUFusion.value);

    commandQueue->destroy();
}

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
        EXPECT_EQ(allocation->getGpuAddress(), bbs->getBatchBufferStartAddress());
    }

    auto itorBBE = find<MI_BATCH_BUFFER_END *>(itorCurrent, cmdList.end());
    EXPECT_NE(cmdList.end(), itorBBE);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueExecuteCommandLists, givenFenceWhenExecutingCmdListThenFenceStatusIsCorrect) {
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
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    *csr.tagAddress = 10;
    csr.taskCount = 10;

    ze_fence_desc_t fenceDesc{};
    auto fence = whitebox_cast(Fence::create(commandQueue, &fenceDesc));
    ASSERT_NE(nullptr, fence);
    EXPECT_EQ(ZE_RESULT_NOT_READY, fence->queryStatus());

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, fence, true);
    *csr.tagAddress = 11;
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(*csr.tagAddress, fence->taskCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, fence->queryStatus());

    //reset fence
    fence->assignTaskCountFromCsr();
    EXPECT_EQ(ZE_RESULT_NOT_READY, fence->queryStatus());

    fence->destroy();
    commandQueue->destroy();
}

HWTEST2_F(CommandQueueExecuteCommandLists, whenUsingFenceThenExpectEndingPipeControlUpdatingTagAllocation, IsGen9) {
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

HWTEST2_F(CommandQueueExecuteCommandLists, givenMidThreadPreemptionWhenCommandsAreExecutedTwoTimesThenStateSipIsAddedOnlyTheFirstTime, IsAtLeastSkl) {
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

HWTEST2_F(CommandQueueExecuteCommandLists, givenCommandListsWithCooperativeAndNonCooperativeKernelsWhenExecuteCommandListsIsCalledThenErrorIsReturned, IsAtLeastSkl) {
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

    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowMixingRegularAndCooperativeKernels.set(1);
    {
        ze_command_list_handle_t commandLists[] = {pCommandListWithCooperativeKernels->toHandle(),
                                                   pCommandListWithNonCooperativeKernels->toHandle()};
        auto result = pCommandQueue->executeCommandLists(2, commandLists, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
    {
        ze_command_list_handle_t commandLists[] = {pCommandListWithNonCooperativeKernels->toHandle(),
                                                   pCommandListWithCooperativeKernels->toHandle()};
        auto result = pCommandQueue->executeCommandLists(2, commandLists, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
    pCommandQueue->destroy();
}

HWTEST2_F(CommandQueueExecuteCommandLists, givenCommandListWithCooperativeKernelsWhenExecuteCommandListsIsCalledThenCorrectBatchBufferIsSubmitted, IsAtLeastXeHpCore) {
    struct MockCsr : NEO::CommandStreamReceiverHw<FamilyType> {
        using NEO::CommandStreamReceiverHw<FamilyType>::CommandStreamReceiverHw;
        NEO::SubmissionStatus submitBatchBuffer(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
            useSingleSubdeviceValue = batchBuffer.useSingleSubdevice;
            submitBatchBufferCalled++;
            return NEO::CommandStreamReceiver::submitBatchBuffer(batchBuffer, allocationsForResidency);
        }
        bool useSingleSubdeviceValue = false;
        uint32_t submitBatchBufferCalled = 0;
    };

    NEO::UltDeviceFactory deviceFactory{1, 4};
    auto pNeoDevice = deviceFactory.rootDevices[0];

    ze_command_queue_desc_t desc = {};
    MockCsr *pMockCsr = new MockCsr{*pNeoDevice->getExecutionEnvironment(), pNeoDevice->getRootDeviceIndex(), pNeoDevice->getDeviceBitfield()};
    pNeoDevice->resetCommandStreamReceiver(pMockCsr);

    Mock<L0::DeviceImp> device{pNeoDevice, pNeoDevice->getExecutionEnvironment()};
    auto pCommandQueue = new MockCommandQueueHw<gfxCoreFamily>{&device, pMockCsr, &desc};
    pCommandQueue->initialize(false, false);

    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(&device, nullptr));
    kernel.module = pMockModule.get();

    ze_group_count_t threadGroupDimensions{1, 1, 1};
    auto pCommandListWithCooperativeKernels = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandListWithCooperativeKernels->initialize(&device, NEO::EngineGroupType::Compute, 0u);
    pCommandListWithCooperativeKernels->appendLaunchKernelWithParams(&kernel, &threadGroupDimensions, nullptr, false, false, true);
    ze_command_list_handle_t commandListCooperative[] = {pCommandListWithCooperativeKernels->toHandle()};
    auto result = pCommandQueue->executeCommandLists(1, commandListCooperative, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, pMockCsr->submitBatchBufferCalled);
    EXPECT_TRUE(pMockCsr->useSingleSubdeviceValue);

    auto pCommandListWithNonCooperativeKernels = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    pCommandListWithNonCooperativeKernels->initialize(&device, NEO::EngineGroupType::Compute, 0u);
    pCommandListWithNonCooperativeKernels->appendLaunchKernelWithParams(&kernel, &threadGroupDimensions, nullptr, false, false, false);
    ze_command_list_handle_t commandListNonCooperative[] = {pCommandListWithNonCooperativeKernels->toHandle()};
    result = pCommandQueue->executeCommandLists(1, commandListNonCooperative, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, pMockCsr->submitBatchBufferCalled);
    EXPECT_FALSE(pMockCsr->useSingleSubdeviceValue);

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

HWTEST2_F(CommandQueueExecuteCommandLists, GivenCmdListsWithDifferentPreemptionModesWhenExecutingThenQueuePreemptionIsSwitchedAndStateSipProgrammedOnce, IsAtLeastSkl) {
    twoCommandListCommandPreemptionTest<FamilyType>(false);
}

HWTEST2_F(CommandQueueExecuteCommandLists, GivenCmdListsWithDifferentPreemptionModesWhenNoCmdStreamPreemptionRequiredThenNoCmdStreamProgrammingAndStateSipProgrammedOnce, IsAtLeastSkl) {
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

template <typename GfxFamily>
void findPartitionRegister(GenCmdList &cmdList, bool expectToFind) {
    using MI_LOAD_REGISTER_MEM = typename GfxFamily::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    auto loadRegisterMemList = findAll<MI_LOAD_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    bool wparidRegisterFound = false;
    for (size_t i = 0; i < loadRegisterMemList.size(); i++) {
        auto loadRegMem = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(*loadRegisterMemList[i]);
        if (NEO::PartitionRegisters<GfxFamily>::wparidCCSOffset == loadRegMem->getRegisterAddress()) {
            wparidRegisterFound = true;
        }
    }

    auto loadRegisterImmList = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    bool offsetRegisterFound = false;
    for (size_t i = 0; i < loadRegisterImmList.size(); i++) {
        auto loadRegImm = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*loadRegisterImmList[i]);
        if (NEO::PartitionRegisters<GfxFamily>::addressOffsetCCSOffset == loadRegImm->getRegisterOffset()) {
            offsetRegisterFound = true;
        }
    }

    if (expectToFind) {
        EXPECT_TRUE(wparidRegisterFound);
        EXPECT_TRUE(offsetRegisterFound);
    } else {
        EXPECT_FALSE(wparidRegisterFound);
        EXPECT_FALSE(offsetRegisterFound);
    }
}

HWTEST2_F(MultiDeviceCommandQueueExecuteCommandLists, givenMultiplePartitionCountWhenExecutingCmdListThenExpectMmioProgrammingAndCorrectEstimation, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;
    using PARSE = typename FamilyType::PARSE;

    auto neoDevice = device->getNEODevice();
    auto csr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);
    csr->useNotifyEnableForPostSync = true;

    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    ze_result_t returnValue;

    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(2u, commandQueue->partitionCount);
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto &commandStreamReceiver = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    if (neoDevice->getPreemptionMode() == PreemptionMode::MidThread || neoDevice->isDebuggerActive()) {
        commandStreamReceiver->createPreemptionAllocation();
    }

    ze_fence_desc_t fenceDesc{};
    auto fence = whitebox_cast(Fence::create(commandQueue, &fenceDesc));
    ASSERT_NE(nullptr, fence);
    ze_fence_handle_t fenceHandle = fence->toHandle();

    //1st execute call initialized pipeline
    auto usedSpaceBefore = commandQueue->commandStream->getUsed();
    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, fenceHandle, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    //1st call then initialize registers
    GenCmdList cmdList;
    ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), usedSpaceBefore), usedSpaceAfter));
    findPartitionRegister<FamilyType>(cmdList, true);

    usedSpaceBefore = commandQueue->commandStream->getUsed();
    result = commandQueue->executeCommandLists(numCommandLists, commandLists, fenceHandle, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);
    size_t cmdBufferSizeWithoutMmioProgramming = usedSpaceAfter - usedSpaceBefore;

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->partitionCount = 2;
    }

    cmdList.clear();
    ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), usedSpaceBefore), usedSpaceAfter));
    findPartitionRegister<FamilyType>(cmdList, false);

    usedSpaceBefore = commandQueue->commandStream->getUsed();
    result = commandQueue->executeCommandLists(numCommandLists, commandLists, fenceHandle, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);
    size_t cmdBufferSizeWithtMmioProgramming = usedSpaceAfter - usedSpaceBefore;

    size_t expectedSizeWithMmioProgramming = cmdBufferSizeWithoutMmioProgramming;
    EXPECT_GE(expectedSizeWithMmioProgramming, cmdBufferSizeWithtMmioProgramming);

    cmdList.clear();
    ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), usedSpaceBefore), usedSpaceAfter));
    findPartitionRegister<FamilyType>(cmdList, false);

    auto pipeControlList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    uint32_t foundPostSyncPipeControl = 0u;
    for (size_t i = 0; i < pipeControlList.size(); i++) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControlList[i]);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_TRUE(pipeControl->getWorkloadPartitionIdOffsetEnable());
            foundPostSyncPipeControl++;
            EXPECT_TRUE(pipeControl->getNotifyEnable());
        }
    }
    EXPECT_EQ(1u, foundPostSyncPipeControl);

    fence->destroy();
    commandQueue->destroy();
}

HWTEST_F(CommandQueueExecuteCommandLists, GivenCopyCommandQueueWhenExecutingCopyCommandListWithFenceThenExpectSingleCopyPostSyncCommand) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    using PARSE = typename FamilyType::PARSE;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto csr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);
    csr->useNotifyEnableForPostSync = true;

    const ze_command_queue_desc_t desc{};
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily,
                                                           device,
                                                           neoDevice->getDefaultEngine().commandStreamReceiver,
                                                           &desc,
                                                           true,
                                                           false,
                                                           returnValue));
    ASSERT_NE(nullptr, commandQueue);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_fence_desc_t fenceDesc{};
    auto fence = whitebox_cast(Fence::create(commandQueue, &fenceDesc));
    ASSERT_NE(nullptr, fence);
    ze_fence_handle_t fenceHandle = fence->toHandle();

    zet_command_list_handle_t cmdListHandle = commandList->toHandle();
    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, fenceHandle, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    size_t usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList, commandQueue->commandStream->getCpuBase(), usedSpaceAfter));

    uint32_t foundPostSyncMiFlush = 0u;
    auto miFlushList = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());
    for (auto cmdIt : miFlushList) {
        auto miFlush = reinterpret_cast<MI_FLUSH_DW *>(*cmdIt);

        if (miFlush->getPostSyncOperation() == MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD) {
            foundPostSyncMiFlush++;
            EXPECT_TRUE(miFlush->getNotifyEnable());
        }
    }
    EXPECT_EQ(1u, foundPostSyncMiFlush);

    fence->destroy();
    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
