/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_logical_state_helper.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"

namespace L0 {
namespace ult {

using CommandQueueCreate = Test<DeviceFixture>;

TEST_F(CommandQueueCreate, whenCreatingCommandQueueThenItIsInitialized) {
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));

    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    ASSERT_NE(nullptr, commandQueue);

    size_t commandStreamSize = MemoryConstants::kiloByte * 128u;
    ASSERT_NE(nullptr, commandQueue->commandStream);
    EXPECT_EQ(commandStreamSize, commandQueue->commandStream->getMaxAvailableSpace());
    EXPECT_EQ(commandQueue->buffers.getCurrentBufferAllocation(), commandQueue->commandStream->getGraphicsAllocation());
    EXPECT_LT(0u, commandQueue->commandStream->getAvailableSpace());

    EXPECT_EQ(csr.get(), commandQueue->getCsr());
    EXPECT_EQ(device, commandQueue->getDevice());
    EXPECT_EQ(0u, commandQueue->getTaskCount());
    EXPECT_NE(nullptr, commandQueue->buffers.getCurrentBufferAllocation());

    size_t expectedCommandBufferAllocationSize = commandStreamSize + MemoryConstants::cacheLineSize + NEO::CSRequirements::csOverfetchSize;
    expectedCommandBufferAllocationSize = alignUp(expectedCommandBufferAllocationSize, MemoryConstants::pageSize64k);

    size_t actualCommandBufferSize = commandQueue->buffers.getCurrentBufferAllocation()->getUnderlyingBufferSize();
    EXPECT_EQ(expectedCommandBufferAllocationSize, actualCommandBufferSize);

    returnValue = commandQueue->destroy();
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
}

TEST_F(CommandQueueCreate, whenSynchronizeByPollingTaskCountThenCallsPrintOutputOnPrintfFunctionsStoredAndClearsFunctionContainer) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));

    Mock<Kernel> kernel1, kernel2;

    commandQueue->printfFunctionContainer.push_back(&kernel1);
    commandQueue->printfFunctionContainer.push_back(&kernel2);

    commandQueue->synchronizeByPollingForTaskCount(0u);

    EXPECT_EQ(0u, commandQueue->printfFunctionContainer.size());
    EXPECT_EQ(1u, kernel1.printPrintfOutputCalledTimes);
    EXPECT_EQ(1u, kernel2.printPrintfOutputCalledTimes);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenGpuHangOnSecondReserveWhenReservingLinearStreamThenReturnGpuHang) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    NEO::WaitStatus waitStatus{NEO::WaitStatus::NotReady};

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));

    size_t maxSize = commandQueue->commandStream->getMaxAvailableSpace();

    auto firstAllocation = commandQueue->commandStream->getGraphicsAllocation();
    EXPECT_EQ(firstAllocation, commandQueue->buffers.getCurrentBufferAllocation());

    uint32_t currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;
    csr.waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::Ready;

    commandQueue->commandStream->getSpace(maxSize - 16u);
    commandQueue->buffers.setCurrentFlushStamp(121u, 121u);
    size_t nextSize = 32u;

    waitStatus = commandQueue->reserveLinearStreamSize(nextSize);
    EXPECT_EQ(NEO::WaitStatus::Ready, waitStatus);

    csr.waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::GpuHang;
    commandQueue->commandStream->getSpace(maxSize - 32u);
    commandQueue->buffers.setCurrentFlushStamp(128u, 128u);
    nextSize = 64u;

    waitStatus = commandQueue->reserveLinearStreamSize(nextSize);
    EXPECT_EQ(NEO::WaitStatus::GpuHang, waitStatus);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, whenReserveLinearStreamThenBufferAllocationSwitched) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    NEO::WaitStatus waitStatus{NEO::WaitStatus::NotReady};

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));

    size_t maxSize = commandQueue->commandStream->getMaxAvailableSpace();

    auto firstAllocation = commandQueue->commandStream->getGraphicsAllocation();
    EXPECT_EQ(firstAllocation, commandQueue->buffers.getCurrentBufferAllocation());

    uint32_t currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;

    commandQueue->commandStream->getSpace(maxSize - 16u);
    commandQueue->buffers.setCurrentFlushStamp(121u, 121u);
    size_t nextSize = 16u + 16u;

    waitStatus = commandQueue->reserveLinearStreamSize(nextSize);
    EXPECT_EQ(NEO::WaitStatus::Ready, waitStatus);

    auto secondAllocation = commandQueue->commandStream->getGraphicsAllocation();
    EXPECT_EQ(secondAllocation, commandQueue->buffers.getCurrentBufferAllocation());
    EXPECT_NE(firstAllocation, secondAllocation);
    EXPECT_EQ(csr.latestWaitForCompletionWithTimeoutTaskCount, currentTaskCount);

    commandQueue->commandStream->getSpace(maxSize - 16u);
    commandQueue->buffers.setCurrentFlushStamp(244u, 244u);

    waitStatus = commandQueue->reserveLinearStreamSize(nextSize);
    EXPECT_EQ(NEO::WaitStatus::Ready, waitStatus);

    auto thirdAllocation = commandQueue->commandStream->getGraphicsAllocation();
    EXPECT_EQ(thirdAllocation, commandQueue->buffers.getCurrentBufferAllocation());
    EXPECT_EQ(thirdAllocation, firstAllocation);
    EXPECT_NE(thirdAllocation, secondAllocation);
    EXPECT_EQ(csr.latestWaitForCompletionWithTimeoutTaskCount, 121u);

    commandQueue->commandStream->getSpace(maxSize - 16u);

    waitStatus = commandQueue->reserveLinearStreamSize(nextSize);
    EXPECT_EQ(NEO::WaitStatus::Ready, waitStatus);

    auto fourthAllocation = commandQueue->commandStream->getGraphicsAllocation();
    EXPECT_EQ(fourthAllocation, commandQueue->buffers.getCurrentBufferAllocation());
    EXPECT_EQ(fourthAllocation, secondAllocation);
    EXPECT_NE(fourthAllocation, firstAllocation);
    EXPECT_EQ(csr.latestWaitForCompletionWithTimeoutTaskCount, 244u);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, whenCreatingCommandQueueWithInvalidProductFamilyThenFailureIsReturned) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    L0::CommandQueue *commandQueue = CommandQueue::create(PRODUCT_FAMILY::IGFX_MAX_PRODUCT,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);

    ASSERT_EQ(nullptr, commandQueue);
}

TEST_F(CommandQueueCreate, whenCmdBuffersAllocationsAreCreatedThenSizeIsNotLessThanQueuesLinearStreamSize) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));

    size_t maxSize = commandQueue->commandStream->getMaxAvailableSpace();

    auto sizeFirstBuffer = commandQueue->buffers.getCurrentBufferAllocation()->getUnderlyingBufferSize();
    EXPECT_LE(maxSize, sizeFirstBuffer);

    commandQueue->commandStream->getSpace(maxSize - 16u);
    size_t nextSize = 16u + 16u;

    const auto waitStatus = commandQueue->reserveLinearStreamSize(nextSize);
    EXPECT_EQ(NEO::WaitStatus::Ready, waitStatus);

    auto sizeSecondBuffer = commandQueue->buffers.getCurrentBufferAllocation()->getUnderlyingBufferSize();
    EXPECT_LE(maxSize, sizeSecondBuffer);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, given100CmdListsWhenExecutingThenCommandStreamIsNotDepleted) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    ASSERT_NE(nullptr, commandQueue);

    Mock<Kernel> kernel;
    kernel.immutableData.device = device;

    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);

    ze_group_count_t dispatchFunctionArguments{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel.toHandle(), &dispatchFunctionArguments, nullptr, 0, nullptr, launchParams);

    const size_t numHandles = 100;
    ze_command_list_handle_t cmdListHandles[numHandles];
    for (size_t i = 0; i < numHandles; i++) {
        cmdListHandles[i] = commandList->toHandle();
    }

    auto sizeBefore = commandQueue->commandStream->getUsed();
    commandQueue->executeCommandLists(numHandles, cmdListHandles, nullptr, false);
    auto sizeAfter = commandQueue->commandStream->getUsed();
    EXPECT_LT(sizeBefore, sizeAfter);

    size_t streamSizeMinimum =
        sizeof(MI_BATCH_BUFFER_END) +
        numHandles * sizeof(MI_BATCH_BUFFER_START);

    EXPECT_LE(streamSizeMinimum, sizeAfter - sizeBefore);

    size_t maxSize = 2 * streamSizeMinimum;
    EXPECT_GT(maxSize, sizeAfter - sizeBefore);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenLogicalStateHelperWhenExecutingThenMergeStates) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;

    auto mockCsrLogicalStateHelper = new NEO::LogicalStateHelperMock<FamilyType>();
    auto mockCmdListLogicalStateHelper = new NEO::LogicalStateHelperMock<FamilyType>();

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->logicalStateHelper.reset(mockCsrLogicalStateHelper);

    Mock<Kernel> kernel;
    kernel.immutableData.device = device;

    auto commandList = std::unique_ptr<L0::ult::CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList->nonImmediateLogicalStateHelper.reset(mockCmdListLogicalStateHelper);

    ze_group_count_t dispatchFunctionArguments{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel.toHandle(), &dispatchFunctionArguments, nullptr, 0, nullptr, launchParams);

    ze_command_list_handle_t cmdListHandles = commandList->toHandle();

    commandQueue->executeCommandLists(1, &cmdListHandles, nullptr, false);

    EXPECT_EQ(1u, mockCsrLogicalStateHelper->mergePipelinedStateCounter);
    EXPECT_EQ(mockCmdListLogicalStateHelper, mockCsrLogicalStateHelper->latestInputLogicalStateHelperForMerge);
    EXPECT_EQ(0u, mockCmdListLogicalStateHelper->mergePipelinedStateCounter);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenLogicalStateHelperAndImmediateCmdListWhenExecutingThenMergeStates) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;

    auto mockCsrLogicalStateHelper = new NEO::LogicalStateHelperMock<FamilyType>();

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->logicalStateHelper.reset(mockCsrLogicalStateHelper);

    Mock<Kernel> kernel;
    kernel.immutableData.device = device;

    auto commandList = std::unique_ptr<L0::ult::CommandList>(whiteboxCast(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue)));

    ze_group_count_t dispatchFunctionArguments{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel.toHandle(), &dispatchFunctionArguments, nullptr, 0, nullptr, launchParams);

    ze_command_list_handle_t cmdListHandles = commandList->toHandle();

    commandQueue->executeCommandLists(1, &cmdListHandles, nullptr, false);

    EXPECT_EQ(0u, mockCsrLogicalStateHelper->mergePipelinedStateCounter);
    EXPECT_EQ(nullptr, mockCsrLogicalStateHelper->latestInputLogicalStateHelperForMerge);

    commandQueue->destroy();
}

HWTEST2_F(CommandQueueCreate, givenGpuHangInReservingLinearStreamWhenExecutingCommandListsThenDeviceLostIsReturned, IsSKL) {
    const ze_command_queue_desc_t desc = {};
    MockCommandQueueHw<gfxCoreFamily> commandQueue(device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc);
    commandQueue.reserveLinearStreamSizeReturnValue = NEO::WaitStatus::GpuHang;

    Mock<Kernel> kernel;
    kernel.immutableData.device = device;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);

    ze_group_count_t dispatchFunctionArguments{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel.toHandle(), &dispatchFunctionArguments, nullptr, 0, nullptr, launchParams);

    ze_command_list_handle_t cmdListHandles[1] = {commandList->toHandle()};

    const auto result = commandQueue.executeCommandLists(1, cmdListHandles, nullptr, false);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);
}

HWTEST_F(CommandQueueCreate, givenUpdateTaskCountFromWaitAndRegularCmdListWhenDispatchTaskCountWriteThenPipeControlFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    DebugManagerStateRestore restorer;
    DebugManager.flags.UpdateTaskCountFromWait.set(3);

    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));

    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), commandQueue->commandStream->getUsed()));

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    bool pipeControlsPostSync = false;
    for (size_t i = 0; i < pipeControls.size(); i++) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControls[i]);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            pipeControlsPostSync = true;
        }
    }
    EXPECT_TRUE(pipeControlsPostSync);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenUpdateTaskCountFromWaitAndImmediateCmdListWhenDispatchTaskCountWriteThenNoPipeControlFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    DebugManagerStateRestore restorer;
    DebugManager.flags.UpdateTaskCountFromWait.set(3);

    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));

    auto commandList = CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue);
    ASSERT_NE(nullptr, commandList);

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), commandQueue->commandStream->getUsed()));

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    bool pipeControlsPostSync = false;
    for (size_t i = 0; i < pipeControls.size(); i++) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControls[i]);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            pipeControlsPostSync = true;
        }
    }
    EXPECT_FALSE(pipeControlsPostSync);

    commandList->destroy();
    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenContainerWithAllocationsWhenResidencyContainerIsEmptyThenMakeResidentWasNotCalled) {
    auto csr = std::make_unique<MockCommandStreamReceiver>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    ResidencyContainer container;
    uint32_t peekTaskCountBefore = commandQueue->csr->peekTaskCount();
    uint32_t flushedTaskCountBefore = commandQueue->csr->peekLatestFlushedTaskCount();
    NEO::SubmissionStatus ret = commandQueue->submitBatchBuffer(0, container, nullptr, false);
    EXPECT_EQ(csr->makeResidentCalledTimes, 0u);
    EXPECT_EQ(ret, NEO::SubmissionStatus::SUCCESS);
    EXPECT_EQ((peekTaskCountBefore + 1), commandQueue->csr->peekTaskCount());
    EXPECT_EQ((flushedTaskCountBefore + 1), commandQueue->csr->peekLatestFlushedTaskCount());
    EXPECT_EQ(commandQueue->commandStream->getGraphicsAllocation()->getTaskCount(commandQueue->csr->getOsContext().getContextId()), commandQueue->csr->peekTaskCount());
    EXPECT_EQ(commandQueue->commandStream->getGraphicsAllocation()->getResidencyTaskCount(commandQueue->csr->getOsContext().getContextId()), commandQueue->csr->peekTaskCount());
    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenCommandStreamReceiverFailsThenSubmitBatchBufferReturnsError) {
    auto csr = std::make_unique<MockCommandStreamReceiverWithFailingSubmitBatch>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    ResidencyContainer container;
    uint32_t peekTaskCountBefore = commandQueue->csr->peekTaskCount();
    uint32_t flushedTaskCountBefore = commandQueue->csr->peekLatestFlushedTaskCount();
    NEO::SubmissionStatus ret = commandQueue->submitBatchBuffer(0, container, nullptr, false);
    EXPECT_EQ(ret, NEO::SubmissionStatus::FAILED);
    EXPECT_EQ(peekTaskCountBefore, commandQueue->csr->peekTaskCount());
    EXPECT_EQ(flushedTaskCountBefore, commandQueue->csr->peekLatestFlushedTaskCount());
    EXPECT_EQ(commandQueue->commandStream->getGraphicsAllocation()->getTaskCount(commandQueue->csr->getOsContext().getContextId()), commandQueue->csr->peekTaskCount());
    EXPECT_EQ(commandQueue->commandStream->getGraphicsAllocation()->getResidencyTaskCount(commandQueue->csr->getOsContext().getContextId()), commandQueue->csr->peekTaskCount());
    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenOutOfMemoryThenSubmitBatchBufferReturnsOutOfMemoryError) {
    auto csr = std::make_unique<MockCommandStreamReceiverWithOutOfMemorySubmitBatch>(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    ResidencyContainer container;
    NEO::SubmissionStatus ret = commandQueue->submitBatchBuffer(0, container, nullptr, false);
    EXPECT_EQ(ret, NEO::SubmissionStatus::OUT_OF_MEMORY);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, whenCommandQueueCreatedThenExpectLinearStreamInitializedWithExpectedSize) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    ASSERT_NE(commandQueue, nullptr);

    size_t commandStreamSize = MemoryConstants::kiloByte * 128u;
    EXPECT_EQ(commandStreamSize, commandQueue->commandStream->getMaxAvailableSpace());

    size_t expectedCommandBufferAllocationSize = commandStreamSize + MemoryConstants::cacheLineSize + NEO::CSRequirements::csOverfetchSize;
    expectedCommandBufferAllocationSize = alignUp(expectedCommandBufferAllocationSize, MemoryConstants::pageSize64k);
    size_t actualCommandBufferSize = commandQueue->buffers.getCurrentBufferAllocation()->getUnderlyingBufferSize();
    EXPECT_EQ(expectedCommandBufferAllocationSize, actualCommandBufferSize);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenQueueInAsyncModeAndRugularCmdListWithAppendBarrierThenFlushTaskIsNotUsed) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    ASSERT_NE(nullptr, commandQueue);

    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);

    commandList->appendBarrier(nullptr, 0, nullptr);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenQueueInSyncModeAndRugularCmdListWithAppendBarrierThenFlushTaskIsNotUsed) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    ASSERT_NE(nullptr, commandQueue);

    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);

    commandList->appendBarrier(nullptr, 0, nullptr);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCmdQueueWithBlitCopyWhenExecutingNonCopyBlitCommandListThenWrongCommandListStatusReturned) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(status, ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCmdQueueWithBlitCopyWhenExecutingCopyBlitCommandListThenSuccessReturned) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          true,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Copy, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(status, ZE_RESULT_SUCCESS);

    commandQueue->destroy();
}

using DeviceCreateCommandQueueTest = Test<DeviceFixture>;
TEST_F(DeviceCreateCommandQueueTest, givenLowPriorityDescWhenCreateCommandQueueIsCalledThenLowPriorityCsrIsAssigned) {
    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;

    ze_command_queue_handle_t commandQueueHandle = {};

    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_TRUE(commandQueue->getCsr()->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForLowPriority(&csr);
    EXPECT_EQ(commandQueue->getCsr(), csr);
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest, givenCopyOrdinalWhenCreateCommandQueueWithLowPriorityDescIsCalledThenCopyCsrIsAssigned) {
    auto copyCsr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    EngineDescriptor copyEngineDescriptor({aub_stream::ENGINE_BCS, EngineUsage::Regular}, neoDevice->getDeviceBitfield(), neoDevice->getPreemptionMode(), false, false);
    auto copyOsContext = neoDevice->getExecutionEnvironment()->memoryManager->createAndRegisterOsContext(copyCsr.get(), copyEngineDescriptor);
    copyCsr->setupContext(*copyOsContext);

    auto computeCsr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    EngineDescriptor computeEngineDescriptor({aub_stream::ENGINE_CCS, EngineUsage::LowPriority}, neoDevice->getDeviceBitfield(), neoDevice->getPreemptionMode(), false, false);
    auto computeOsContext = neoDevice->getExecutionEnvironment()->memoryManager->createAndRegisterOsContext(computeCsr.get(), computeEngineDescriptor);
    computeCsr->setupContext(*computeOsContext);

    auto &engineGroups = neoDevice->getRegularEngineGroups();
    engineGroups.clear();

    auto &allEngines = const_cast<std::vector<NEO::EngineControl> &>(neoDevice->getAllEngines());
    allEngines.clear();

    engineGroups.push_back(NEO::EngineGroupT{});
    engineGroups.back().engineGroupType = EngineGroupType::Copy;
    engineGroups.back().engines.resize(1);
    engineGroups.back().engines[0].commandStreamReceiver = copyCsr.get();
    EngineControl copyEngine{copyCsr.get(), copyOsContext};
    allEngines.push_back(copyEngine);

    engineGroups.push_back(NEO::EngineGroupT{});
    engineGroups.back().engineGroupType = EngineGroupType::Compute;
    engineGroups.back().engines.resize(1);
    engineGroups.back().engines[0].commandStreamReceiver = computeCsr.get();
    EngineControl computeEngine{computeCsr.get(), computeOsContext};
    allEngines.push_back(computeEngine);

    uint32_t count = 0u;
    ze_result_t res = device->getCommandQueueGroupProperties(&count, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_GT(count, 0u);

    std::vector<ze_command_queue_group_properties_t> properties(count);
    res = device->getCommandQueueGroupProperties(&count, properties.data());
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    uint32_t ordinal = 0u;
    for (ordinal = 0u; ordinal < count; ordinal++) {
        if ((properties[ordinal].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COPY) &&
            !(properties[ordinal].flags & ZE_COMMAND_QUEUE_GROUP_PROPERTY_FLAG_COMPUTE)) {
            if (properties[ordinal].numQueues == 0)
                continue;
            break;
        }
    }
    EXPECT_LT(ordinal, count);

    ze_command_queue_desc_t desc{};
    desc.ordinal = ordinal;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;

    ze_command_queue_handle_t commandQueueHandle = {};

    res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_EQ(copyCsr.get(), commandQueue->getCsr());
    commandQueue->destroy();

    engineGroups.clear();
    allEngines.clear();
}

struct DeferredContextCreationDeviceCreateCommandQueueTest : DeviceCreateCommandQueueTest {
    void SetUp() override {
        DebugManager.flags.DeferOsContextInitialization.set(1);
        DeviceCreateCommandQueueTest::SetUp();
    }

    DebugManagerStateRestore restore;
};

TEST_F(DeferredContextCreationDeviceCreateCommandQueueTest, givenLowPriorityEngineNotInitializedWhenCreateLowPriorityCommandQueueIsCalledThenEngineIsInitialized) {
    NEO::CommandStreamReceiver *lowPriorityCsr = nullptr;
    device->getCsrForLowPriority(&lowPriorityCsr);
    ASSERT_FALSE(lowPriorityCsr->getOsContext().isInitialized());

    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;
    ze_command_queue_handle_t commandQueueHandle = {};
    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_TRUE(lowPriorityCsr->getOsContext().isInitialized());

    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest, givenNormalPriorityDescWhenCreateCommandQueueIsCalledWithValidArgumentThenCsrIsAssignedWithOrdinalAndIndex) {
    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    ze_command_queue_handle_t commandQueueHandle = {};

    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_FALSE(commandQueue->getCsr()->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    EXPECT_EQ(commandQueue->getCsr(), csr);
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest,
       whenCallingGetCsrForOrdinalAndIndexWithInvalidOrdinalThenInvalidArgumentIsReturned) {
    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    ze_command_queue_handle_t commandQueueHandle = {};

    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_FALSE(commandQueue->getCsr()->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    res = device->getCsrForOrdinalAndIndex(&csr, std::numeric_limits<uint32_t>::max(), 0u);
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest,
       whenCallingGetCsrForOrdinalAndIndexWithInvalidIndexThenInvalidArgumentIsReturned) {
    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    ze_command_queue_handle_t commandQueueHandle = {};

    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_FALSE(commandQueue->getCsr()->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    res = device->getCsrForOrdinalAndIndex(&csr, 0u, std::numeric_limits<uint32_t>::max());
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest, givenLowPriorityDescAndWithoutLowPriorityCsrWhenCreateCommandQueueIsCalledThenAbortIsThrown) {
    // remove low priority EngineControl objects for negative testing
    neoDevice->allEngines.erase(std::remove_if(
        neoDevice->allEngines.begin(),
        neoDevice->allEngines.end(),
        [](EngineControl &p) { return p.osContext->isLowPriority(); }));

    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;

    ze_command_queue_handle_t commandQueueHandle = {};

    EXPECT_THROW(device->createCommandQueue(&desc, &commandQueueHandle), std::exception);
}

struct MultiDeviceCreateCommandQueueFixture : MultiDeviceFixture {
    void setUp() {
        DebugManager.flags.EnableImplicitScaling = false;
        MultiDeviceFixture::setUp();
    }
};

using MultiDeviceCreateCommandQueueTest = Test<MultiDeviceCreateCommandQueueFixture>;

TEST_F(MultiDeviceCreateCommandQueueTest, givenLowPriorityDescWhenCreateCommandQueueIsCalledThenLowPriorityCsrIsAssigned) {
    auto device = driverHandle->devices[0];

    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;

    ze_command_queue_handle_t commandQueueHandle = {};

    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_TRUE(commandQueue->getCsr()->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForLowPriority(&csr);
    EXPECT_EQ(commandQueue->getCsr(), csr);
    commandQueue->destroy();
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandQueue : public L0::CommandQueueHw<gfxCoreFamily> {
  public:
    using L0::CommandQueueHw<gfxCoreFamily>::CommandQueueHw;

    MockCommandQueue(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : L0::CommandQueueHw<gfxCoreFamily>(device, csr, desc) {}
    using BaseClass = ::L0::CommandQueueHw<gfxCoreFamily>;

    using BaseClass::csr;
    using BaseClass::heapContainer;

    NEO::HeapContainer mockHeapContainer;
    void handleScratchSpace(NEO::HeapContainer &heapContainer,
                            NEO::ScratchSpaceController *scratchController,
                            bool &gsbaState, bool &frontEndState,
                            uint32_t perThreadScratchSpaceSize,
                            uint32_t perThreadPrivateScratchSize) override {
        this->mockHeapContainer = heapContainer;
    }

    void programFrontEnd(uint64_t scratchAddress, uint32_t perThreadScratchSpaceSize, NEO::LinearStream &commandStream) override {
        return;
    }
};

using ExecuteCommandListTests = Test<DeviceFixture>;
HWTEST2_F(ExecuteCommandListTests, givenExecuteCommandListWhenItReturnsThenContainersAreEmpty, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueue<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    commandList->setCommandListPerThreadScratchSize(100u);
    auto commandListHandle = commandList->toHandle();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, NEO::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, NEO::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);

    commandList->commandContainer.sshAllocations.push_back(&graphicsAllocation1);
    commandList->commandContainer.sshAllocations.push_back(&graphicsAllocation2);

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(0u, commandQueue->csr->getResidencyAllocations().size());
    EXPECT_EQ(0u, commandQueue->heapContainer.size());

    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandQueueSubmitBatchBuffer : public MockCommandQueue<gfxCoreFamily> {
  public:
    MockCommandQueueSubmitBatchBuffer(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : MockCommandQueue<gfxCoreFamily>(device, csr, desc) {}

    ADDMETHOD_NOBASE(submitBatchBuffer, NEO::SubmissionStatus, NEO::SubmissionStatus::SUCCESS,
                     (size_t offset, NEO::ResidencyContainer &residencyContainer, void *endingCmdPtr,
                      bool isCooperative));
};

HWTEST2_F(ExecuteCommandListTests, givenOutOfMemorySubmitBatchBufferThenExecuteCommandListReturnsOutOfMemoryError, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueueSubmitBatchBuffer<gfxCoreFamily>(device, csr, &desc);
    commandQueue->submitBatchBufferResult = NEO::SubmissionStatus::OUT_OF_MEMORY;

    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    auto commandListHandle = commandList->toHandle();

    auto res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);

    commandQueue->destroy();
    commandList->destroy();
}

using CommandQueueDestroy = Test<DeviceFixture>;
HWTEST2_F(CommandQueueDestroy, givenCommandQueueAndCommandListWithSshAndScratchWhenExecuteThenSshWasUsed, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueue<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    commandList->setCommandListPerThreadScratchSize(100u);
    auto commandListHandle = commandList->toHandle();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, NEO::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, NEO::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);

    commandList->commandContainer.sshAllocations.push_back(&graphicsAllocation1);
    commandList->commandContainer.sshAllocations.push_back(&graphicsAllocation2);

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(commandQueue->mockHeapContainer.size(), 3u);
    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

HWTEST2_F(CommandQueueDestroy, givenCommandQueueAndCommandListWithSshAndPrivateScratchWhenExecuteThenSshWasUsed, IsAtLeastXeHpCore) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueue<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    commandList->setCommandListPerThreadPrivateScratchSize(100u);
    auto commandListHandle = commandList->toHandle();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, NEO::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, NEO::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);

    commandList->commandContainer.sshAllocations.push_back(&graphicsAllocation1);
    commandList->commandContainer.sshAllocations.push_back(&graphicsAllocation2);

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(commandQueue->mockHeapContainer.size(), 3u);
    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

HWTEST2_F(CommandQueueDestroy, givenCommandQueueAndCommandListWithWhenBindlessEnabledThenHeapContainerIsEmpty, IsAtLeastSkl) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.UseBindlessMode.set(1);
    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice->getMemoryManager(), neoDevice->getNumGenericSubDevices() > 1, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueue<gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    commandList->setCommandListPerThreadScratchSize(100u);
    auto commandListHandle = commandList->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    EXPECT_EQ(commandQueue->mockHeapContainer.size(), 0u);
    commandQueue->destroy();
    commandList->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenFailingSubmitBatchBufferThenExecuteCommandListReturnsErrorUnknown, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueueSubmitBatchBuffer<gfxCoreFamily>(device, csr, &desc);
    commandQueue->submitBatchBufferResult = NEO::SubmissionStatus::FAILED;

    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    auto commandListHandle = commandList->toHandle();

    auto res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);

    commandQueue->destroy();
    commandList->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenFailingSubmitBatchBufferThenResetGraphicsTaskCountsLatestFlushedTaskCountZero, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};

    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);

    auto commandQueue = new MockCommandQueueSubmitBatchBuffer<gfxCoreFamily>(device, csr, &desc);
    commandQueue->submitBatchBufferResult = NEO::SubmissionStatus::FAILED;

    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    auto commandListHandle = commandList->toHandle();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, NEO::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, NEO::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);
    commandList->commandContainer.addToResidencyContainer(&graphicsAllocation1);
    commandList->commandContainer.addToResidencyContainer(&graphicsAllocation2);
    static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(csr)->taskCount = 0;
    auto res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);

    EXPECT_EQ(NEO::GraphicsAllocation::objectNotUsed, graphicsAllocation1.getTaskCount(csr->getOsContext().getContextId()));
    EXPECT_EQ(NEO::GraphicsAllocation::objectNotUsed, graphicsAllocation2.getTaskCount(csr->getOsContext().getContextId()));
    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

HWTEST2_F(ExecuteCommandListTests, givenFailingSubmitBatchBufferThenResetGraphicsTaskCountsLatestFlushedTaskCountNonZero, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};

    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);

    auto commandQueue = new MockCommandQueueSubmitBatchBuffer<gfxCoreFamily>(device, csr, &desc);
    commandQueue->submitBatchBufferResult = NEO::SubmissionStatus::FAILED;

    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    auto commandListHandle = commandList->toHandle();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, NEO::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, NEO::AllocationType::BUFFER, alloc, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);
    commandList->commandContainer.addToResidencyContainer(&graphicsAllocation1);
    commandList->commandContainer.addToResidencyContainer(&graphicsAllocation2);
    static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(csr)->taskCount = 2;
    auto res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);

    EXPECT_EQ(2u, graphicsAllocation1.getTaskCount(csr->getOsContext().getContextId()));
    EXPECT_EQ(2u, graphicsAllocation2.getTaskCount(csr->getOsContext().getContextId()));
    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

HWTEST2_F(ExecuteCommandListTests, givenFailingSubmitBatchBufferThenWaitForCompletionFalse, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueueSubmitBatchBuffer<gfxCoreFamily>(device, csr, &desc);
    commandQueue->submitBatchBufferResult = NEO::SubmissionStatus::FAILED;

    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    auto commandListHandle = commandList->toHandle();
    uint32_t flushedTaskCountPrior = csr->peekTaskCount();
    csr->setLatestFlushedTaskCount(flushedTaskCountPrior);
    auto res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);
    EXPECT_EQ(csr->peekLatestFlushedTaskCount(), flushedTaskCountPrior);

    commandQueue->destroy();
    commandList->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenSuccessfulSubmitBatchBufferThenExecuteCommandListReturnsSuccess, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = new MockCommandQueueSubmitBatchBuffer<gfxCoreFamily>(device, csr, &desc);
    commandQueue->submitBatchBufferResult = NEO::SubmissionStatus::SUCCESS;

    commandQueue->initialize(false, false);
    auto commandList = new CommandListCoreFamily<gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    auto commandListHandle = commandList->toHandle();

    auto res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    commandQueue->destroy();
    commandList->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenCommandQueueHavingTwoB2BCommandListsThenMVSDirtyFlagAndGSBADirtyFlagAreSetOnlyOnce, IsAtLeastSkl) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = CommandQueue::create(productFamily,
                                             device,
                                             csr,
                                             &desc,
                                             false,
                                             false,
                                             returnValue);
    auto commandList0 = new CommandListCoreFamily<gfxCoreFamily>();
    commandList0->initialize(device, NEO::EngineGroupType::Compute, 0u);
    commandList0->setCommandListPerThreadScratchSize(0u);
    auto commandList1 = new CommandListCoreFamily<gfxCoreFamily>();
    commandList1->initialize(device, NEO::EngineGroupType::Compute, 0u);
    commandList1->setCommandListPerThreadScratchSize(0u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    EXPECT_EQ(true, csr->getMediaVFEStateDirty());
    EXPECT_EQ(true, csr->getGSBAStateDirty());
    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(false, csr->getMediaVFEStateDirty());
    EXPECT_EQ(false, csr->getGSBAStateDirty());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(false, csr->getMediaVFEStateDirty());
    EXPECT_EQ(false, csr->getGSBAStateDirty());

    commandQueue->destroy();
    commandList0->destroy();
    commandList1->destroy();
}

using CommandQueueExecuteSupport = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;
HWTEST2_F(ExecuteCommandListTests, givenCommandQueueHavingTwoB2BCommandListsThenMVSIsProgrammedOnlyOnce, CommandQueueExecuteSupport) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList0->setCommandListPerThreadScratchSize(0u);
    auto commandList1 = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList1->setCommandListPerThreadScratchSize(0u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    ASSERT_NE(nullptr, commandQueue->commandStream);

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList1.begin(), cmdList1.end());
    auto gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList1.begin(), cmdList1.end());
    // We should have only 1 state added
    ASSERT_EQ(1u, mediaVfeStates.size());
    ASSERT_EQ(1u, gsbaStates.size());

    commandQueue->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenTwoCommandQueuesHavingTwoB2BCommandListsWithPTSSsetForFirstCmdListThenMVSAndGSBAAreProgrammedOnlyOnce, CommandQueueExecuteSupport) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    auto commandList1 = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList0->setCommandListPerThreadScratchSize(512u);
    commandList1->setCommandListPerThreadScratchSize(0u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have only 1 state added
    ASSERT_EQ(1u, mediaVfeStates.size());
    ASSERT_EQ(1u, gsbaStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(0u);
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(0u);

    auto commandQueue1 = whiteboxCast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));

    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    usedSpaceAfter = commandQueue1->commandStream->getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream->getCpuBase(), 0), usedSpaceAfter));

    mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList1.begin(), cmdList1.end());
    gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList1.begin(), cmdList1.end());
    // We should have no state added
    ASSERT_EQ(0u, mediaVfeStates.size());
    ASSERT_EQ(0u, gsbaStates.size());

    commandQueue->destroy();
    commandQueue1->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenTwoCommandQueuesHavingTwoB2BCommandListsAndWithPTSSsetForSecondCmdListThenMVSandGSBAAreProgrammedTwice, CommandQueueExecuteSupport) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    auto commandList1 = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList0->setCommandListPerThreadScratchSize(0u);
    commandList1->setCommandListPerThreadScratchSize(512u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(0u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have 2 states added
    ASSERT_EQ(2u, mediaVfeStates.size());
    ASSERT_EQ(2u, gsbaStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(512u);
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(0u);

    auto commandQueue1 = whiteboxCast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));

    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    usedSpaceAfter = commandQueue1->commandStream->getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream->getCpuBase(), 0), usedSpaceAfter));

    mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList1.begin(), cmdList1.end());
    gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList1.begin(), cmdList1.end());
    // We should have no state added
    ASSERT_EQ(0u, mediaVfeStates.size());
    ASSERT_EQ(0u, gsbaStates.size());

    commandQueue->destroy();
    commandQueue1->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenTwoCommandQueuesHavingTwoB2BCommandListsAndWithPTSSGrowingThenMVSAndGSBAAreProgrammedTwice, CommandQueueExecuteSupport) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    auto commandList1 = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList0->setCommandListPerThreadScratchSize(512u);
    commandList1->setCommandListPerThreadScratchSize(512u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have only 1 state added
    ASSERT_EQ(1u, mediaVfeStates.size());
    ASSERT_EQ(1u, gsbaStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(1024u);
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(1024u);

    auto commandQueue1 = whiteboxCast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));

    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(1024u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(1024u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    usedSpaceAfter = commandQueue1->commandStream->getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream->getCpuBase(), 0), usedSpaceAfter));

    mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList1.begin(), cmdList1.end());
    gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList1.begin(), cmdList1.end());
    // We should have only 1 state added
    ASSERT_EQ(1u, mediaVfeStates.size());
    ASSERT_EQ(1u, gsbaStates.size());

    commandQueue->destroy();
    commandQueue1->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenTwoCommandQueuesHavingTwoB2BCommandListsAndWithPTSSUniquePerCmdListThenMVSAndGSBAAreProgrammedOncePerSubmission, CommandQueueExecuteSupport) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    auto commandList1 = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList0->setCommandListPerThreadScratchSize(0u);
    commandList1->setCommandListPerThreadScratchSize(512u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(0u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have 2 states added
    ASSERT_EQ(2u, mediaVfeStates.size());
    ASSERT_EQ(2u, gsbaStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(1024u);
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(2048u);

    auto commandQueue1 = whiteboxCast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(1024u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(2048u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSize());

    usedSpaceAfter = commandQueue1->commandStream->getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream->getCpuBase(), 0), usedSpaceAfter));

    mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList1.begin(), cmdList1.end());
    gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList1.begin(), cmdList1.end());
    // We should have 2 states added
    ASSERT_EQ(2u, mediaVfeStates.size());
    ASSERT_EQ(2u, gsbaStates.size());

    commandQueue->destroy();
    commandQueue1->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenTwoCommandQueuesHavingTwoB2BCommandListsAndWithPrivateScratchUniquePerCmdListThenCFEIsProgrammedOncePerSubmission, IsAtLeastXeHpCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    auto commandList1 = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList0->setCommandListPerThreadPrivateScratchSize(0u);
    commandList1->setCommandListPerThreadPrivateScratchSize(512u);
    auto commandListHandle0 = commandList0->toHandle();
    auto commandListHandle1 = commandList1->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(0u, csr->getScratchSpaceController()->getPerThreadPrivateScratchSize());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadPrivateScratchSize());

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());

    ASSERT_EQ(2u, mediaVfeStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadPrivateScratchSize(1024u);
    commandList1->reset();
    commandList1->setCommandListPerThreadPrivateScratchSize(2048u);

    auto commandQueue1 = whiteboxCast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           returnValue));
    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false);
    EXPECT_EQ(1024u, csr->getScratchSpaceController()->getPerThreadPrivateScratchSize());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false);
    EXPECT_EQ(2048u, csr->getScratchSpaceController()->getPerThreadPrivateScratchSize());

    usedSpaceAfter = commandQueue1->commandStream->getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream->getCpuBase(), 0), usedSpaceAfter));

    mediaVfeStates = findAll<CFE_STATE *>(cmdList1.begin(), cmdList1.end());

    ASSERT_EQ(2u, mediaVfeStates.size());

    commandQueue->destroy();
    commandQueue1->destroy();
}

HWTEST_F(ExecuteCommandListTests, givenDirectSubmissionEnabledWhenExecutingCmdListThenSetNonZeroBatchBufferStartAddress) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(csr)->directSubmissionAvailable = true;
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList->setCommandListPerThreadPrivateScratchSize(0u);
    auto commandListHandle = commandList->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto bbStartCmds = findAll<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());

    ASSERT_EQ(2u, bbStartCmds.size());

    for (auto &cmd : bbStartCmds) {
        auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*cmd);

        EXPECT_NE(0u, bbStart->getBatchBufferStartAddress());
    }

    commandQueue->destroy();
}

HWTEST_F(ExecuteCommandListTests, givenDirectSubmissionEnabledAndDebugFlagSetWhenExecutingCmdListThenSetZeroBatchBufferStartAddress) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.BatchBufferStartPrepatchingWaEnabled.set(0);

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(csr)->directSubmissionAvailable = true;
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    commandList->setCommandListPerThreadPrivateScratchSize(0u);
    auto commandListHandle = commandList->toHandle();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto bbStartCmds = findAll<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(2u, bbStartCmds.size());

    for (auto &cmd : bbStartCmds) {
        auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*cmd);
        if (cmd == bbStartCmds.back()) {
            EXPECT_EQ(0u, bbStart->getBatchBufferStartAddress());
        } else {
            EXPECT_NE(0u, bbStart->getBatchBufferStartAddress());
        }
    }

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenOverrideCmdQueueSyncModeToDefaultWhenCommandQueueIsCreatedWithSynchronousModeThenDefaultModeIsSelected) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.OverrideCmdQueueSynchronousMode.set(0);

    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_ERROR_DEVICE_LOST;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    auto cmdQueueSynchronousMode = reinterpret_cast<L0::CommandQueueImp *>(commandQueue)->getSynchronousMode();
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_DEFAULT, cmdQueueSynchronousMode);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenOverrideCmdQueueSyncModeToAsynchronousWhenCommandQueueIsCreatedWithSynchronousModeThenAsynchronousModeIsSelected) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.OverrideCmdQueueSynchronousMode.set(2);

    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_ERROR_DEVICE_LOST;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    auto cmdQueueSynchronousMode = reinterpret_cast<L0::CommandQueueImp *>(commandQueue)->getSynchronousMode();
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, cmdQueueSynchronousMode);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenOverrideCmdQueueSyncModeToSynchronousWhenCommandQueueIsCreatedWithAsynchronousModeThenSynchronousModeIsSelected) {
    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.OverrideCmdQueueSynchronousMode.set(1);

    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_ERROR_DEVICE_LOST;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    auto cmdQueueSynchronousMode = reinterpret_cast<L0::CommandQueueImp *>(commandQueue)->getSynchronousMode();
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, cmdQueueSynchronousMode);

    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
