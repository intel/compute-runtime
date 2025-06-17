/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_fence.h"

namespace L0 {
namespace ult {

using CommandQueueExecuteCommandListsSimpleTest = Test<DeviceFixture>;

HWTEST_F(CommandQueueExecuteCommandListsSimpleTest, GivenSynchronousModeWhenExecutingCommandListThenSynchronizeIsCalled) {
    ze_command_queue_desc_t desc;
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQ->initialize(false, false, false);
    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    CommandList::fromHandle(commandLists[0])->close();
    mockCmdQ->executeCommandLists(1, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(mockCmdQ->synchronizedCalled, 1u);
    CommandList::fromHandle(commandLists[0])->destroy();
    mockCmdQ->destroy();
}

HWTEST_F(CommandQueueExecuteCommandListsSimpleTest, GivenSynchronousModeAndDeviceLostSynchronizeWhenExecutingCommandListThenSynchronizeIsCalledAndDeviceLostIsReturned) {
    ze_command_queue_desc_t desc;
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    auto mockCmdQ = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQ->initialize(false, false, false);
    mockCmdQ->synchronizeReturnValue = ZE_RESULT_ERROR_DEVICE_LOST;

    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    CommandList::fromHandle(commandLists[0])->close();
    const auto result = mockCmdQ->executeCommandLists(1, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(mockCmdQ->synchronizedCalled, 1u);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);

    CommandList::fromHandle(commandLists[0])->destroy();
    mockCmdQ->destroy();
}

HWTEST_F(CommandQueueExecuteCommandListsSimpleTest, GivenAsynchronousModeWhenExecutingCommandListThenSynchronizeIsNotCalled) {
    ze_command_queue_desc_t desc;
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    auto mockCmdQ = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc);
    mockCmdQ->initialize(false, false, false);
    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    CommandList::fromHandle(commandLists[0])->close();
    mockCmdQ->executeCommandLists(1, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(mockCmdQ->synchronizedCalled, 0u);
    CommandList::fromHandle(commandLists[0])->destroy();
    mockCmdQ->destroy();
}

HWTEST_F(CommandQueueExecuteCommandListsSimpleTest, whenUsingFenceThenLastPipeControlUpdatesFenceAllocation) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    ze_fence_desc_t fenceDesc = {};

    auto fence = whiteboxCast(Fence::create(commandQueue, &fenceDesc));
    ze_fence_handle_t fenceHandle = fence->toHandle();

    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle(),
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    CommandList::fromHandle(commandLists[0])->close();
    CommandList::fromHandle(commandLists[1])->close();
    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, fenceHandle, true, nullptr, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

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

HWTEST2_F(CommandQueueExecuteCommandListsSimpleTest, givenTwoCommandQueuesUsingSingleCsrWhenExecutingFirstTimeOnBothThenPipelineSelectProgrammedOnce, IsAtMostXeCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto &productHelper = device->getProductHelper();
    bool additionalPipelineSelect = productHelper.is3DPipelineSelectWARequired() &&
                                    neoDevice->getDefaultEngine().commandStreamReceiver->isRcs();

    ze_result_t returnValue;

    ze_command_list_handle_t commandList = CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandQueue);

    CommandList::fromHandle(commandList)->close();
    auto usedSpaceBefore = commandQueue->commandStream.getUsed();
    returnValue = commandQueue->executeCommandLists(1, &commandList, nullptr, false, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandQueue->commandStream.getCpuBase(), usedSpaceBefore),
        usedSpaceAfter - usedSpaceBefore));

    auto pipelineSelect = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    size_t expectedFirstPipelineSelectCount = 1u;
    if (additionalPipelineSelect) {
        expectedFirstPipelineSelectCount += 2;
    }
    EXPECT_EQ(expectedFirstPipelineSelectCount, pipelineSelect.size());

    cmdList.clear();

    auto commandQueue2 = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandQueue2);

    usedSpaceBefore = commandQueue2->commandStream.getUsed();
    returnValue = commandQueue2->executeCommandLists(1, &commandList, nullptr, false, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    usedSpaceAfter = commandQueue2->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandQueue2->commandStream.getCpuBase(), usedSpaceBefore),
        usedSpaceAfter - usedSpaceBefore));

    pipelineSelect = findAll<PIPELINE_SELECT *>(cmdList.begin(), cmdList.end());
    constexpr size_t expectedSecondPipelineSelectCount = 0u;
    EXPECT_EQ(expectedSecondPipelineSelectCount, pipelineSelect.size());

    CommandList::fromHandle(commandList)->destroy();
    commandQueue->destroy();
    commandQueue2->destroy();
}

HWTEST2_F(CommandQueueExecuteCommandListsSimpleTest, givenTwoCommandQueuesUsingSingleCsrWhenExecutingFirstTimeOnBothQueuesThenPreemptionModeIsProgrammedOnce, IsAtMostXeCore) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    ze_result_t returnValue;

    ze_command_list_handle_t commandList = CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle();
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    CommandList::whiteboxCast(CommandList::fromHandle(commandList))->commandListPreemptionMode = NEO::PreemptionMode::ThreadGroup;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandQueue);

    CommandList::fromHandle(commandList)->close();
    auto usedSpaceBefore = commandQueue->commandStream.getUsed();
    returnValue = commandQueue->executeCommandLists(1, &commandList, nullptr, false, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandQueue->commandStream.getCpuBase(), usedSpaceBefore),
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

    auto commandQueue2 = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandQueue2);

    usedSpaceBefore = commandQueue2->commandStream.getUsed();
    returnValue = commandQueue2->executeCommandLists(1, &commandList, nullptr, false, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    usedSpaceAfter = commandQueue2->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandQueue2->commandStream.getCpuBase(), usedSpaceBefore),
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

struct PauseOnGpuFixture : public Test<ModuleFixture> {
    void setUp() {
        ModuleFixture::setUp();

        auto &csr = neoDevice->getGpgpuCommandStreamReceiver();
        debugPauseStateAddress = csr.getDebugPauseStateGPUAddress();

        createKernel();
    }

    void TearDown() override {
        commandList->destroy();
        commandQueue->destroy();
        ModuleFixture::tearDown();
    }

    void createImmediateCommandList() {
        commandList->destroy();

        ze_command_queue_desc_t queueDesc = {};
        ze_result_t returnValue;
        commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue);
        ASSERT_NE(nullptr, commandList);
        commandListHandle = commandList->toHandle();
    }

    template <typename FamilyType>
    bool verifySemaphore(const GenCmdList::iterator &iterator, uint64_t debugPauseStateAddress, DebugPauseState requiredDebugPauseState) {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
        auto semaphoreCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*iterator);

        if ((static_cast<uint32_t>(requiredDebugPauseState) == semaphoreCmd->getSemaphoreDataDword()) &&
            (debugPauseStateAddress == semaphoreCmd->getSemaphoreGraphicsAddress())) {

            EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, semaphoreCmd->getCompareOperation());
            EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, semaphoreCmd->getWaitMode());

            return true;
        }

        return false;
    }

    template <typename FamilyType>
    bool verifyPipeControl(const GenCmdList::iterator &iterator, uint64_t debugPauseStateAddress, DebugPauseState requiredDebugPauseState) {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

        auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*iterator);

        if ((static_cast<uint32_t>(requiredDebugPauseState) == pipeControlCmd->getImmediateData()) &&
            (debugPauseStateAddress == NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControlCmd))) {
            EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
            EXPECT_EQ(MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, this->device->getNEODevice()->getRootDeviceEnvironment()), pipeControlCmd->getDcFlushEnable());
            EXPECT_EQ(PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, pipeControlCmd->getPostSyncOperation());
            return true;
        }

        return false;
    }

    template <typename FamilyType>
    bool verifyLoadRegImm(const GenCmdList::iterator &iterator) {
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
        uint32_t expectedRegisterOffset = debugManager.flags.GpuScratchRegWriteRegisterOffset.get();
        uint32_t expectedRegisterData = debugManager.flags.GpuScratchRegWriteRegisterData.get();
        auto loadRegImm = genCmdCast<MI_LOAD_REGISTER_IMM *>(*iterator);

        if ((expectedRegisterOffset == loadRegImm->getRegisterOffset()) &&
            (expectedRegisterData == loadRegImm->getDataDword())) {
            return true;
        }

        return false;
    }

    template <typename FamilyType>
    void findSemaphores(GenCmdList &cmdList) {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
        auto semaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        while (semaphore != cmdList.end()) {
            if (verifySemaphore<FamilyType>(semaphore, debugPauseStateAddress, DebugPauseState::hasUserStartConfirmation)) {
                semaphoreBeforeWalkerFound++;
            }

            if (verifySemaphore<FamilyType>(semaphore, debugPauseStateAddress, DebugPauseState::hasUserEndConfirmation)) {
                semaphoreAfterWalkerFound++;
            }

            semaphore = find<MI_SEMAPHORE_WAIT *>(++semaphore, cmdList.end());
        }
    }

    template <typename FamilyType>
    void findPipeControls(GenCmdList &cmdList) {
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        auto pipeControl = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

        while (pipeControl != cmdList.end()) {
            if (verifyPipeControl<FamilyType>(pipeControl, debugPauseStateAddress, DebugPauseState::waitingForUserStartConfirmation)) {
                pipeControlBeforeWalkerFound++;
            }

            if (verifyPipeControl<FamilyType>(pipeControl, debugPauseStateAddress, DebugPauseState::waitingForUserEndConfirmation)) {
                pipeControlAfterWalkerFound++;
            }

            pipeControl = find<PIPE_CONTROL *>(++pipeControl, cmdList.end());
        }
    }

    void enqueueKernel() {
        auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        result = commandList->close();
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        result = commandQueue->executeCommandLists(1u, &commandListHandle, nullptr, false, nullptr, nullptr);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }

    DebugManagerStateRestore restore;

    CmdListKernelLaunchParams launchParams = {};
    ze_group_count_t groupCount{1, 1, 1};

    L0::ult::CommandQueue *commandQueue = nullptr;
    L0::CommandList *commandList = nullptr;
    ze_command_list_handle_t commandListHandle = {};

    uint64_t debugPauseStateAddress = 0;

    uint32_t semaphoreBeforeWalkerFound = 0;
    uint32_t semaphoreAfterWalkerFound = 0;
    uint32_t pipeControlBeforeWalkerFound = 0;
    uint32_t pipeControlAfterWalkerFound = 0;
};

struct PauseOnGpuTests : public PauseOnGpuFixture {
    void SetUp() override {
        PauseOnGpuFixture::setUp();

        ze_command_queue_desc_t queueDesc = {};
        ze_result_t returnValue;
        commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
        ASSERT_NE(nullptr, commandQueue);

        commandList = CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false);
        ASSERT_NE(nullptr, commandList);
        commandListHandle = commandList->toHandle();
    }

    void enqueueKernel() {
        auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        result = commandList->close();
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        result = commandQueue->executeCommandLists(1u, &commandListHandle, nullptr, false, nullptr, nullptr);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }
};

HWTEST_F(PauseOnGpuTests, givenPauseOnEnqueueFlagSetWhenDispatchWalkersThenInsertPauseCommandsAroundSpecifiedEnqueue) {
    debugManager.flags.PauseOnEnqueue.set(1);

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    enqueueKernel();
    enqueueKernel();

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    findSemaphores<FamilyType>(cmdList);
    findPipeControls<FamilyType>(cmdList);

    EXPECT_EQ(1u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(1u, semaphoreAfterWalkerFound);
    EXPECT_EQ(1u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(1u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseOnEnqueueFlagSetToAlwaysWhenDispatchWalkersThenInsertPauseCommandsAroundEachEnqueue) {
    debugManager.flags.PauseOnEnqueue.set(-2);

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    enqueueKernel();
    enqueueKernel();

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    findSemaphores<FamilyType>(cmdList);
    findPipeControls<FamilyType>(cmdList);

    EXPECT_EQ(2u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(2u, semaphoreAfterWalkerFound);
    EXPECT_EQ(2u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(2u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseModeSetToBeforeOnlyWhenDispatchingThenInsertPauseOnlyBeforeEnqueue) {
    debugManager.flags.PauseOnEnqueue.set(0);
    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeWorkload);

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    enqueueKernel();
    enqueueKernel();

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    findSemaphores<FamilyType>(cmdList);

    findPipeControls<FamilyType>(cmdList);

    EXPECT_EQ(1u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(0u, semaphoreAfterWalkerFound);
    EXPECT_EQ(1u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(0u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseModeSetToAfterOnlyWhenDispatchingThenInsertPauseOnlyAfterEnqueue) {
    debugManager.flags.PauseOnEnqueue.set(0);
    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::AfterWorkload);

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    enqueueKernel();

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    findSemaphores<FamilyType>(cmdList);
    findPipeControls<FamilyType>(cmdList);

    EXPECT_EQ(0u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(1u, semaphoreAfterWalkerFound);
    EXPECT_EQ(0u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(1u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuTests, givenPauseModeSetToBeforeAndAfterWhenDispatchingThenInsertPauseAroundEnqueue) {
    debugManager.flags.PauseOnEnqueue.set(0);
    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeAndAfterWorkload);

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    enqueueKernel();

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    findSemaphores<FamilyType>(cmdList);

    findPipeControls<FamilyType>(cmdList);

    EXPECT_EQ(1u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(1u, semaphoreAfterWalkerFound);
    EXPECT_EQ(1u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(1u, pipeControlAfterWalkerFound);
}

struct PauseOnGpuWithImmediateCommandListTests : public PauseOnGpuFixture {
    void SetUp() override {
        PauseOnGpuFixture::setUp();

        ze_command_queue_desc_t queueDesc = {};
        ze_result_t returnValue;
        commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
        ASSERT_NE(nullptr, commandQueue);

        commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue);
        ASSERT_NE(nullptr, commandList);
        commandListHandle = commandList->toHandle();
    }

    void enqueueKernel() {
        auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }
};

HWTEST_F(PauseOnGpuWithImmediateCommandListTests, givenPauseOnEnqueueFlagSetWhenDispatchWalkersThenInsertPauseCommandsAroundSpecifiedEnqueue) {
    debugManager.flags.PauseOnEnqueue.set(1);

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    enqueueKernel();
    enqueueKernel();

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    findSemaphores<FamilyType>(cmdList);
    findPipeControls<FamilyType>(cmdList);

    EXPECT_EQ(1u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(1u, semaphoreAfterWalkerFound);
    EXPECT_EQ(1u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(1u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuWithImmediateCommandListTests, givenPauseOnEnqueueFlagSetToAlwaysWhenDispatchWalkersThenInsertPauseCommandsAroundEachEnqueue) {
    debugManager.flags.PauseOnEnqueue.set(-2);

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    enqueueKernel();
    enqueueKernel();

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    findSemaphores<FamilyType>(cmdList);
    findPipeControls<FamilyType>(cmdList);

    EXPECT_EQ(2u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(2u, semaphoreAfterWalkerFound);
    EXPECT_EQ(2u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(2u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuWithImmediateCommandListTests, givenPauseModeSetToBeforeOnlyWhenDispatchingThenInsertPauseOnlyBeforeEnqueue) {
    debugManager.flags.PauseOnEnqueue.set(0);
    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeWorkload);

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    enqueueKernel();
    enqueueKernel();

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    findSemaphores<FamilyType>(cmdList);

    findPipeControls<FamilyType>(cmdList);

    EXPECT_EQ(1u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(0u, semaphoreAfterWalkerFound);
    EXPECT_EQ(1u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(0u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuWithImmediateCommandListTests, givenPauseModeSetToAfterOnlyWhenDispatchingThenInsertPauseOnlyAfterEnqueue) {
    debugManager.flags.PauseOnEnqueue.set(0);
    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::AfterWorkload);

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    enqueueKernel();

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    findSemaphores<FamilyType>(cmdList);
    findPipeControls<FamilyType>(cmdList);

    EXPECT_EQ(0u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(1u, semaphoreAfterWalkerFound);
    EXPECT_EQ(0u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(1u, pipeControlAfterWalkerFound);
}

HWTEST_F(PauseOnGpuWithImmediateCommandListTests, givenPauseModeSetToBeforeAndAfterWhenDispatchingThenInsertPauseAroundEnqueue) {
    debugManager.flags.PauseOnEnqueue.set(0);
    debugManager.flags.PauseOnGpuMode.set(PauseOnGpuProperties::PauseMode::BeforeAndAfterWorkload);

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    enqueueKernel();

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    findSemaphores<FamilyType>(cmdList);

    findPipeControls<FamilyType>(cmdList);

    EXPECT_EQ(1u, semaphoreBeforeWalkerFound);
    EXPECT_EQ(1u, semaphoreAfterWalkerFound);
    EXPECT_EQ(1u, pipeControlBeforeWalkerFound);
    EXPECT_EQ(1u, pipeControlAfterWalkerFound);
}

using CmdListPipelineSelectStateTest = Test<CmdListPipelineSelectStateFixture>;

struct SystolicSupport {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsAnyProducts<IGFX_ALDERLAKE_P, IGFX_PVC>::isMatched<productFamily>() || IsXeHpgCore::isMatched<productFamily>();
    }
};

HWTEST2_F(CmdListPipelineSelectStateTest,
          givenAppendSystolicKernelToCommandListWhenExecutingCommandListThenPipelineSelectStateIsTrackedCorrectly, SystolicSupport) {
    testBody<FamilyType>();
}

HWTEST2_F(CmdListPipelineSelectStateTest,
          givenAppendSystolicAndScratchSpaceKernelToSecondCommandListWhenExecutingTwoCommandListsThenPipelineSelectStateIsDispatchedBeforeSecondScratchProgramingBeforeBatch, SystolicSupport) {
    testBodySystolicAndScratchOnSecondCommandList<FamilyType>();
}

HWTEST2_F(CmdListPipelineSelectStateTest,
          givenCmdQueueAndImmediateCmdListUseSameCsrWhenAppendingSystolicKernelOnBothRegularFirstThenPipelineSelectStateIsNotChanged, SystolicSupport) {
    testBodyShareStateRegularImmediate<FamilyType>();
}

HWTEST2_F(CmdListPipelineSelectStateTest,
          givenCmdQueueAndImmediateCmdListUseSameCsrWhenAppendingSystolicKernelOnBothImmediateFirstThenPipelineSelectStateIsNotChanged, SystolicSupport) {
    testBodyShareStateImmediateRegular<FamilyType>();
}

using CmdListThreadArbitrationTest = Test<CmdListThreadArbitrationFixture>;

using ThreadArbitrationSupport = IsProduct<IGFX_PVC>;
HWTEST2_F(CmdListThreadArbitrationTest,
          givenAppendThreadArbitrationKernelToCommandListWhenExecutingCommandListThenStateComputeModeStateIsTrackedCorrectly, ThreadArbitrationSupport) {
    testBody<FamilyType>();
}

using CmdListLargeGrfTest = Test<CmdListLargeGrfFixture>;

struct LargeGrfSupport {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsXeHpgCore::isMatched<productFamily>() || IsPVC::isMatched<productFamily>();
    }
};

HWTEST2_F(CmdListLargeGrfTest,
          givenAppendLargeGrfKernelToCommandListWhenExecutingCommandListThenStateComputeModeStateIsTrackedCorrectly, LargeGrfSupport) {
    testBody<FamilyType>();
}

HWTEST_F(CommandQueueExecuteCommandListsSimpleTest, GivenDirtyFlagForContextInBindlessHelperWhenExecutingCmdListsThenStateCacheInvalidateIsSent) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice, neoDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    bindlessHeapsHelperPtr->stateCacheDirtyForContext.set(commandQueue->getCsr()->getOsContext().getContextId());

    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle(),
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    CommandList::fromHandle(commandLists[0])->close();
    CommandList::fromHandle(commandLists[1])->close();
    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControls.size());

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControls[0]);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControl->getStateCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getRenderTargetCacheFlushEnable());

    EXPECT_FALSE(bindlessHeapsHelperPtr->getStateDirtyForContext(commandQueue->getCsr()->getOsContext().getContextId()));

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

HWTEST_F(CommandQueueExecuteCommandListsSimpleTest, GivenRegisterInstructionCacheFlushWhenExecutingCmdListsThenInstructionCacheInvalidateIsSent) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;

    neoDevice->getDefaultEngine().commandStreamReceiver->registerInstructionCacheFlush();

    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    uint32_t numCommandLists = 1;
    CommandList::fromHandle(commandLists[0])->close();
    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControls.size());

    bool foundInstructionCacheInvalidate = false;
    for (auto pipeControlIT : pipeControls) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControlIT);
        if (pipeControl->getInstructionCacheInvalidateEnable()) {
            foundInstructionCacheInvalidate = true;
            break;
        }
    }

    EXPECT_TRUE(foundInstructionCacheInvalidate);

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

using CommandQueueExecuteCommandListsMultiDeviceTest = Test<MultiDeviceFixture>;

HWTEST_F(CommandQueueExecuteCommandListsMultiDeviceTest, GivenDirtyFlagForContextInBindlessHelperWhenExecutingCmdListsThenStateCacheInvalidateIsSent) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto neoDevice = driverHandle->devices[numRootDevices - 1]->getNEODevice();
    auto device = driverHandle->devices[numRootDevices - 1];

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice, neoDevice->getNumGenericSubDevices() > 1);
    MockBindlesHeapsHelper *bindlessHeapsHelperPtr = bindlessHeapsHelper.get();
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());

    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    uint32_t contextId = commandQueue->getCsr()->getOsContext().getContextId();
    uint32_t contextIdFirst = neoDevice->getMemoryManager()->getFirstContextIdForRootDevice(numRootDevices - 1);
    EXPECT_LE(contextIdFirst, contextId);

    uint32_t firstDeviceFirstContextId = neoDevice->getMemoryManager()->getFirstContextIdForRootDevice(0);
    EXPECT_EQ(0u, firstDeviceFirstContextId);

    bindlessHeapsHelperPtr->stateCacheDirtyForContext.set();

    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle(),
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    CommandList::fromHandle(commandLists[0])->close();
    CommandList::fromHandle(commandLists[1])->close();
    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pipeControls.size());

    auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControls[0]);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControl->getStateCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControl->getRenderTargetCacheFlushEnable());

    EXPECT_FALSE(bindlessHeapsHelperPtr->getStateDirtyForContext(commandQueue->getCsr()->getOsContext().getContextId()));

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

HWTEST_F(CommandQueueExecuteCommandListsSimpleTest, givenRegularCommandListNotClosedWhenExecutingCommandListThenReturnError) {
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    auto engineGroupType = neoDevice->getGfxCoreHelper().getEngineGroupType(neoDevice->getDefaultEngine().getEngineType(),
                                                                            neoDevice->getDefaultEngine().getEngineUsage(), neoDevice->getHardwareInfo());

    auto commandList = CommandList::create(productFamily, device, engineGroupType, 0u, returnValue, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_FALSE(commandList->isClosed());

    auto commandListHandle = commandList->toHandle();

    returnValue = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, returnValue);

    commandList->close();
    EXPECT_TRUE(commandList->isClosed());

    commandList->reset();
    EXPECT_FALSE(commandList->isClosed());

    commandList->destroy();
    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
