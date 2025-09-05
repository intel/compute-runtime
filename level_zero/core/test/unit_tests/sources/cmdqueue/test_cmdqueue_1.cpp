/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

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
                                                          false,
                                                          returnValue));

    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    ASSERT_NE(nullptr, commandQueue);

    size_t commandStreamSize = MemoryConstants::kiloByte * 128u;
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(commandStreamSize, commandQueue->commandStream.getMaxAvailableSpace());
    EXPECT_EQ(commandQueue->buffers.getCurrentBufferAllocation(), commandQueue->commandStream.getGraphicsAllocation());
    EXPECT_LT(0u, commandQueue->commandStream.getAvailableSpace());

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

TEST_F(CommandQueueCreate, whenSynchronizeByPollingTaskCountThenCallsPrintOutputOnPrintfKernelsStoredAndClearsKernelContainer) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));

    std::shared_ptr<Mock<KernelImp>> kernel1{new Mock<KernelImp>{}};
    std::shared_ptr<Mock<KernelImp>> kernel2{new Mock<KernelImp>{}};

    commandQueue->printfKernelContainer.push_back(std::weak_ptr<Kernel>{kernel1});
    commandQueue->printfKernelContainer.push_back(std::weak_ptr<Kernel>{kernel2});

    commandQueue->synchronizeByPollingForTaskCount(0u);

    EXPECT_EQ(0u, commandQueue->printfKernelContainer.size());
    EXPECT_EQ(1u, kernel1->printPrintfOutputCalledTimes);
    EXPECT_EQ(1u, kernel2->printPrintfOutputCalledTimes);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenPrintfKernelAndDetectedHangWhenSynchronizingByPollingThenPrintPrintfOutputAfterHangIsCalled) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));

    std::shared_ptr<Mock<KernelImp>> kernel{new Mock<KernelImp>{}};
    TaskCountType currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.callBaseWaitForCompletionWithTimeout = false;
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;
    csr.returnWaitForCompletionWithTimeout = WaitStatus::gpuHang;

    commandQueue->printfKernelContainer.push_back(std::weak_ptr<Kernel>{kernel});

    commandQueue->synchronizeByPollingForTaskCount(0u);

    EXPECT_EQ(0u, commandQueue->printfKernelContainer.size());
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_TRUE(kernel->hangDetectedPassedToPrintfOutput);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenPrintfKernelAndDetectedHangWhenSynchronizingThenPrintPrintfOutputAfterHangIsCalled) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.OverrideUseKmdWaitFunction.set(1);

    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));

    std::shared_ptr<Mock<KernelImp>> kernel{new Mock<KernelImp>{}};
    TaskCountType currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;
    csr.waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::gpuHang;

    commandQueue->printfKernelContainer.push_back(std::weak_ptr<Kernel>{kernel});
    commandQueue->synchronize(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(0u, commandQueue->printfKernelContainer.size());
    EXPECT_EQ(1u, kernel->printPrintfOutputCalledTimes);
    EXPECT_TRUE(kernel->hangDetectedPassedToPrintfOutput);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, whenSynchronizedThenPollForAubCompletion) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));

    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto numPolls = csr.pollForAubCompletionCalled;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->synchronize(std::numeric_limits<uint64_t>::max()));
    EXPECT_EQ(numPolls + 1, csr.pollForAubCompletionCalled);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenGpuHangOnSecondReserveWhenReservingLinearStreamThenReturnGpuHang) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    NEO::WaitStatus waitStatus{NEO::WaitStatus::notReady};

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));

    size_t maxSize = commandQueue->commandStream.getMaxAvailableSpace();

    auto firstAllocation = commandQueue->commandStream.getGraphicsAllocation();
    EXPECT_EQ(firstAllocation, commandQueue->buffers.getCurrentBufferAllocation());

    TaskCountType currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;
    csr.waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::ready;

    commandQueue->commandStream.getSpace(maxSize - 16u);
    commandQueue->buffers.setCurrentFlushStamp(121u, 121u);
    size_t nextSize = 32u;

    waitStatus = commandQueue->reserveLinearStreamSize(nextSize);
    EXPECT_EQ(NEO::WaitStatus::ready, waitStatus);

    csr.waitForTaskCountWithKmdNotifyFallbackReturnValue = WaitStatus::gpuHang;
    commandQueue->commandStream.getSpace(maxSize - 32u);
    commandQueue->buffers.setCurrentFlushStamp(128u, 128u);
    nextSize = 64u;

    waitStatus = commandQueue->reserveLinearStreamSize(nextSize);
    EXPECT_EQ(NEO::WaitStatus::gpuHang, waitStatus);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, whenReserveLinearStreamThenBufferAllocationSwitched) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    NEO::WaitStatus waitStatus{NEO::WaitStatus::notReady};

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));

    size_t maxSize = commandQueue->commandStream.getMaxAvailableSpace();

    auto firstAllocation = commandQueue->commandStream.getGraphicsAllocation();
    EXPECT_EQ(firstAllocation, commandQueue->buffers.getCurrentBufferAllocation());

    TaskCountType currentTaskCount = 33u;
    auto &csr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.latestWaitForCompletionWithTimeoutTaskCount = currentTaskCount;

    commandQueue->commandStream.getSpace(maxSize - 16u);
    commandQueue->buffers.setCurrentFlushStamp(121u, 121u);
    size_t nextSize = 16u + 16u;

    waitStatus = commandQueue->reserveLinearStreamSize(nextSize);
    EXPECT_EQ(NEO::WaitStatus::ready, waitStatus);

    auto secondAllocation = commandQueue->commandStream.getGraphicsAllocation();
    EXPECT_EQ(secondAllocation, commandQueue->buffers.getCurrentBufferAllocation());
    EXPECT_NE(firstAllocation, secondAllocation);
    EXPECT_EQ(csr.latestWaitForCompletionWithTimeoutTaskCount, currentTaskCount);

    commandQueue->commandStream.getSpace(maxSize - 16u);
    commandQueue->buffers.setCurrentFlushStamp(244u, 244u);

    waitStatus = commandQueue->reserveLinearStreamSize(nextSize);
    EXPECT_EQ(NEO::WaitStatus::ready, waitStatus);

    auto thirdAllocation = commandQueue->commandStream.getGraphicsAllocation();
    EXPECT_EQ(thirdAllocation, commandQueue->buffers.getCurrentBufferAllocation());
    EXPECT_EQ(thirdAllocation, firstAllocation);
    EXPECT_NE(thirdAllocation, secondAllocation);
    EXPECT_EQ(csr.latestWaitForCompletionWithTimeoutTaskCount, 121u);

    commandQueue->commandStream.getSpace(maxSize - 16u);

    waitStatus = commandQueue->reserveLinearStreamSize(nextSize);
    EXPECT_EQ(NEO::WaitStatus::ready, waitStatus);

    auto fourthAllocation = commandQueue->commandStream.getGraphicsAllocation();
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
                                                          false,
                                                          returnValue));

    size_t maxSize = commandQueue->commandStream.getMaxAvailableSpace();

    auto sizeFirstBuffer = commandQueue->buffers.getCurrentBufferAllocation()->getUnderlyingBufferSize();
    EXPECT_LE(maxSize, sizeFirstBuffer);

    commandQueue->commandStream.getSpace(maxSize - 16u);
    size_t nextSize = 16u + 16u;

    const auto waitStatus = commandQueue->reserveLinearStreamSize(nextSize);
    EXPECT_EQ(NEO::WaitStatus::ready, waitStatus);

    auto sizeSecondBuffer = commandQueue->buffers.getCurrentBufferAllocation()->getUnderlyingBufferSize();
    EXPECT_LE(maxSize, sizeSecondBuffer);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, given100CmdListsWhenExecutingThenCommandStreamIsNotDepleted) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);

    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    ASSERT_NE(nullptr, commandQueue);

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    kernel.immutableData.device = device;

    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);

    ze_group_count_t dispatchKernelArguments{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel.toHandle(), dispatchKernelArguments, nullptr, 0, nullptr, launchParams);

    commandList->close();

    const size_t numHandles = 100;
    ze_command_list_handle_t cmdListHandles[numHandles];
    for (size_t i = 0; i < numHandles; i++) {
        cmdListHandles[i] = commandList->toHandle();
    }

    auto sizeBefore = commandQueue->commandStream.getUsed();
    commandQueue->executeCommandLists(numHandles, cmdListHandles, nullptr, false, nullptr, nullptr);
    auto sizeAfter = commandQueue->commandStream.getUsed();
    EXPECT_LT(sizeBefore, sizeAfter);

    size_t streamSizeMinimum =
        sizeof(MI_BATCH_BUFFER_END) +
        numHandles * sizeof(MI_BATCH_BUFFER_START);

    EXPECT_LE(streamSizeMinimum, sizeAfter - sizeBefore);

    size_t maxSize = 2 * streamSizeMinimum;
    EXPECT_GT(maxSize, sizeAfter - sizeBefore);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenOutOfHostMemoryErrorFromSubmitBatchBufferWhenExecutingCommandListsThenOutOfHostMemoryIsReturned) {
    const ze_command_queue_desc_t desc = {};
    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc);
    commandQueue->initialize(false, false, false);
    commandQueue->submitBatchBufferReturnValue = NEO::SubmissionStatus::outOfHostMemory;

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    kernel.immutableData.device = device;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);

    ze_group_count_t dispatchFunctionArguments{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel.toHandle(), dispatchFunctionArguments, nullptr, 0, nullptr, launchParams);
    commandList->close();

    ze_command_list_handle_t cmdListHandles[1] = {commandList->toHandle()};

    const auto result = commandQueue->executeCommandLists(1, cmdListHandles, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY, result);
    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenGpuHangInReservingLinearStreamWhenExecutingCommandListsThenDeviceLostIsReturned) {
    const ze_command_queue_desc_t desc = {};
    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc);
    commandQueue->initialize(false, false, false);
    commandQueue->reserveLinearStreamSizeReturnValue = NEO::WaitStatus::gpuHang;

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    kernel.immutableData.device = device;

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(device->getHwInfo().platform.eProductFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);

    ze_group_count_t dispatchKernelArguments{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel.toHandle(), dispatchKernelArguments, nullptr, 0, nullptr, launchParams);
    commandList->close();

    ze_command_list_handle_t cmdListHandles[1] = {commandList->toHandle()};

    const auto result = commandQueue->executeCommandLists(1, cmdListHandles, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, result);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenSwTagsEnabledWhenPrepareAndSubmitBatchBufferThenLeftoverIsZeroed) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableSWTags.set(1);
    const ze_command_queue_desc_t desc = {};
    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc);
    commandQueue->initialize(false, false, false);
    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);
    auto &commandStream = commandQueue->commandStream;

    auto estimatedSize = 4096u;
    NEO::LinearStream linearStream(commandStream.getSpace(estimatedSize), estimatedSize);
    // fill with random data
    memset(commandStream.getCpuBase(), 0xD, estimatedSize);
    typename MockCommandQueueHw<FamilyType::gfxCoreFamily>::CommandListExecutionContext ctx{};

    commandQueue->prepareAndSubmitBatchBuffer(ctx, linearStream);

    // MI_BATCH_BUFFER END will be added during prepareAndSubmitBatchBuffer
    auto offsetInBytes = sizeof(typename FamilyType::MI_BATCH_BUFFER_END);
    auto isLeftoverZeroed = true;
    for (auto i = offsetInBytes; i < estimatedSize; i++) {
        uint8_t *data = reinterpret_cast<uint8_t *>(commandStream.getCpuBase());
        if (data[i] != 0) {
            isLeftoverZeroed = false;
            break;
        }
    }
    EXPECT_TRUE(isLeftoverZeroed);
    commandQueue->destroy();
}

template <GFXCORE_FAMILY gfxCoreFamily>
struct MockCommandQueueHwEstimateSizeTest : public MockCommandQueueHw<gfxCoreFamily> {
    using CommandListExecutionContext = typename MockCommandQueueHwEstimateSizeTest<gfxCoreFamily>::CommandListExecutionContext;

    MockCommandQueueHwEstimateSizeTest(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc)
        : MockCommandQueueHw<gfxCoreFamily>(device, csr, desc) {}

    ze_result_t makeAlignedChildStreamAndSetGpuBase(NEO::LinearStream &child, size_t requiredSize, CommandListExecutionContext &ctx) override {
        requiredSizeCalled = requiredSize;
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    bool isDispatchTaskCountPostSyncRequired(ze_fence_handle_t hFence, bool containsAnyRegularCmdList, bool containsParentImmediateStream) const override {
        return dispatchTaskCountPostSyncRequired;
    }
    bool dispatchTaskCountPostSyncRequired = false;
    size_t requiredSizeCalled = 0u;
};

HWTEST_F(CommandQueueCreate, GivenDispatchTaskCountPostSyncRequiredWhenExecuteCommandListsThenEstimatedSizeIsCorrect) {
    const ze_command_queue_desc_t desc = {};
    auto commandQueue = new MockCommandQueueHwEstimateSizeTest<FamilyType::gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);
    commandList->close();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    commandQueue->dispatchTaskCountPostSyncRequired = false;
    commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    auto estimatedSizeWithoutBarrier = commandQueue->requiredSizeCalled;

    commandQueue->dispatchTaskCountPostSyncRequired = true;
    commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    auto estimatedSizeWithtBarrier = commandQueue->requiredSizeCalled;

    auto sizeForBarrier = NEO::MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(device->getNEODevice()->getRootDeviceEnvironment(), NEO::PostSyncMode::immediateData);
    EXPECT_GT(sizeForBarrier, 0u);

    EXPECT_EQ(estimatedSizeWithtBarrier, estimatedSizeWithoutBarrier + sizeForBarrier);

    commandQueue->destroy();
}

HWTEST_F(CommandQueueCreate, givenUpdateTaskCountFromWaitAndRegularCmdListWhenDispatchTaskCountWriteThenPipeControlFlushed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

    DebugManagerStateRestore restorer;
    debugManager.flags.UpdateTaskCountFromWait.set(3);

    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));

    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);
    commandList->close();

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), commandQueue->commandStream.getUsed()));

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
                                                          false,
                                                          returnValue));
    commandQueue->startingCmdBuffer = &commandQueue->commandStream;
    ResidencyContainer container;
    TaskCountType peekTaskCountBefore = commandQueue->csr->peekTaskCount();
    TaskCountType flushedTaskCountBefore = commandQueue->csr->peekLatestFlushedTaskCount();
    NEO::SubmissionStatus ret = commandQueue->submitBatchBuffer(0, container, nullptr, false);
    EXPECT_EQ(csr->makeResidentCalledTimes, 0u);
    EXPECT_EQ(ret, NEO::SubmissionStatus::success);
    EXPECT_EQ((peekTaskCountBefore + 1), commandQueue->csr->peekTaskCount());
    EXPECT_EQ((flushedTaskCountBefore + 1), commandQueue->csr->peekLatestFlushedTaskCount());
    EXPECT_EQ(commandQueue->commandStream.getGraphicsAllocation()->getTaskCount(commandQueue->csr->getOsContext().getContextId()), commandQueue->csr->peekTaskCount());
    EXPECT_EQ(commandQueue->commandStream.getGraphicsAllocation()->getResidencyTaskCount(commandQueue->csr->getOsContext().getContextId()), commandQueue->csr->peekTaskCount());
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
                                                          false,
                                                          returnValue));
    commandQueue->startingCmdBuffer = &commandQueue->commandStream;
    ResidencyContainer container;
    TaskCountType peekTaskCountBefore = commandQueue->csr->peekTaskCount();
    TaskCountType flushedTaskCountBefore = commandQueue->csr->peekLatestFlushedTaskCount();
    NEO::SubmissionStatus ret = commandQueue->submitBatchBuffer(0, container, nullptr, false);
    EXPECT_EQ(ret, NEO::SubmissionStatus::failed);
    EXPECT_EQ(peekTaskCountBefore, commandQueue->csr->peekTaskCount());
    EXPECT_EQ(flushedTaskCountBefore, commandQueue->csr->peekLatestFlushedTaskCount());
    EXPECT_EQ(commandQueue->commandStream.getGraphicsAllocation()->getTaskCount(commandQueue->csr->getOsContext().getContextId()), commandQueue->csr->peekTaskCount());
    EXPECT_EQ(commandQueue->commandStream.getGraphicsAllocation()->getResidencyTaskCount(commandQueue->csr->getOsContext().getContextId()), commandQueue->csr->peekTaskCount());
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
                                                          false,
                                                          returnValue));
    commandQueue->startingCmdBuffer = &commandQueue->commandStream;
    ResidencyContainer container;
    NEO::SubmissionStatus ret = commandQueue->submitBatchBuffer(0, container, nullptr, false);
    EXPECT_EQ(ret, NEO::SubmissionStatus::outOfMemory);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, WhenSubmitBatchBufferThenDisableFlatRingBuffer) {
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
                                                          false,
                                                          returnValue));
    commandQueue->startingCmdBuffer = &commandQueue->commandStream;
    ResidencyContainer container;
    NEO::SubmissionStatus ret = commandQueue->submitBatchBuffer(0, container, nullptr, false);

    EXPECT_EQ(ret, NEO::SubmissionStatus::success);
    EXPECT_TRUE(csr->latestFlushedBatchBuffer.disableFlatRingBuffer);

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
                                                          false,
                                                          returnValue));
    ASSERT_NE(commandQueue, nullptr);

    size_t commandStreamSize = MemoryConstants::kiloByte * 128u;
    EXPECT_EQ(commandStreamSize, commandQueue->commandStream.getMaxAvailableSpace());

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
                                                          false,
                                                          returnValue));
    ASSERT_NE(nullptr, commandQueue);

    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);

    commandList->appendBarrier(nullptr, 0, nullptr, false);

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
                                                          false,
                                                          returnValue));
    ASSERT_NE(nullptr, commandQueue);

    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);

    commandList->appendBarrier(nullptr, 0, nullptr, false);

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
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);
    commandList->close();

    auto commandListHandle = commandList->toHandle();
    auto status = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

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
    device->getCsrForLowPriority(&csr, false);
    EXPECT_EQ(commandQueue->getCsr(), csr);
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest, givenCopyOrdinalWhenCreateCommandQueueWithLowPriorityDescIsCalledThenCopyCsrIsAssigned) {
    auto copyCsr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    EngineDescriptor copyEngineDescriptor({aub_stream::ENGINE_BCS, EngineUsage::lowPriority}, neoDevice->getDeviceBitfield(), neoDevice->getPreemptionMode(), false);
    auto copyOsContext = neoDevice->getExecutionEnvironment()->memoryManager->createAndRegisterOsContext(copyCsr.get(), copyEngineDescriptor);
    copyCsr->setupContext(*copyOsContext);

    auto computeCsr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    EngineDescriptor computeEngineDescriptor({aub_stream::ENGINE_CCS, EngineUsage::lowPriority}, neoDevice->getDeviceBitfield(), neoDevice->getPreemptionMode(), false);
    auto computeOsContext = neoDevice->getExecutionEnvironment()->memoryManager->createAndRegisterOsContext(computeCsr.get(), computeEngineDescriptor);
    computeCsr->setupContext(*computeOsContext);

    auto &engineGroups = neoDevice->getRegularEngineGroups();
    engineGroups.clear();

    auto &allEngines = const_cast<std::vector<NEO::EngineControl> &>(neoDevice->getAllEngines());
    allEngines.clear();

    engineGroups.push_back(NEO::EngineGroupT{});
    engineGroups.back().engineGroupType = EngineGroupType::compute;
    engineGroups.back().engines.resize(1);
    engineGroups.back().engines[0].commandStreamReceiver = computeCsr.get();
    EngineControl computeEngine{computeCsr.get(), computeOsContext};
    allEngines.push_back(computeEngine);

    engineGroups.push_back(NEO::EngineGroupT{});
    engineGroups.back().engineGroupType = EngineGroupType::copy;
    engineGroups.back().engines.resize(1);
    engineGroups.back().engines[0].commandStreamReceiver = copyCsr.get();
    EngineControl copyEngine{copyCsr.get(), copyOsContext};
    allEngines.push_back(copyEngine);

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
        debugManager.flags.DeferOsContextInitialization.set(1);
        DeviceCreateCommandQueueTest::SetUp();
    }

    DebugManagerStateRestore restore;
};

TEST_F(DeferredContextCreationDeviceCreateCommandQueueTest, givenLowPriorityEngineNotInitializedWhenCreateLowPriorityCommandQueueIsCalledThenEngineIsInitialized) {
    NEO::CommandStreamReceiver *lowPriorityCsr = nullptr;
    device->getCsrForLowPriority(&lowPriorityCsr, false);
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

using DeviceCreateCommandQueueTest = Test<DeviceFixture>;
TEST_F(DeviceCreateCommandQueueTest, givenLowPriorityDescWhenCreateImmediateCommandListThenLowPriorityCsrIsAssigned) {
    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_LOW;

    ze_command_list_handle_t commandListHandle = {};

    ze_result_t res = device->createCommandListImmediate(&desc, &commandListHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandList = static_cast<CommandListImp *>(L0::CommandList::fromHandle(commandListHandle));

    EXPECT_NE(commandList, nullptr);
    EXPECT_TRUE(commandList->getCsr(false)->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForLowPriority(&csr, false);
    EXPECT_EQ(commandList->getCsr(false), csr);
    commandList->destroy();
}

TEST_F(DeviceCreateCommandQueueTest, givenNormalPriorityDescWhenCreateCommandQueueIsCalledWithValidArgumentThenCsrIsAssignedWithOrdinalAndIndex) {
    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    ze_command_queue_handle_t commandQueueHandle = {};

    neoDevice->disableSecondaryEngines = true;
    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_FALSE(commandQueue->getCsr()->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
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

    neoDevice->disableSecondaryEngines = true;
    ze_result_t res = device->createCommandQueue(&desc, &commandQueueHandle);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue, nullptr);
    EXPECT_FALSE(commandQueue->getCsr()->getOsContext().isLowPriority());
    NEO::CommandStreamReceiver *csr = nullptr;
    res = device->getCsrForOrdinalAndIndex(&csr, std::numeric_limits<uint32_t>::max(), 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
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
    res = device->getCsrForOrdinalAndIndex(&csr, 0u, std::numeric_limits<uint32_t>::max(), ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    EXPECT_EQ(res, ZE_RESULT_ERROR_INVALID_ARGUMENT);
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest, givenLowPriorityDescAndWithoutLowPriorityCsrWhenCreateCommandQueueIsCalledThenErrorReturned) {
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

    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, device->createCommandQueue(&desc, &commandQueueHandle));
}

struct MultiDeviceCreateCommandQueueFixture : MultiDeviceFixture {
    void setUp() {
        debugManager.flags.EnableImplicitScaling = false;
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
    device->getCsrForLowPriority(&csr, false);
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
    using BaseClass::isCopyOnlyCommandQueue;

    NEO::HeapContainer mockHeapContainer;
    void handleScratchSpace(NEO::HeapContainer &heapContainer,
                            NEO::ScratchSpaceController *scratchController,
                            NEO::GraphicsAllocation *globalStatelessAllocation,
                            bool &gsbaState, bool &frontEndState,
                            uint32_t perThreadScratchSpaceSlot0Size,
                            uint32_t perThreadScratchSpaceSlot1Size) override {
        this->mockHeapContainer = heapContainer;
    }

    void programFrontEnd(uint64_t scratchAddress, uint32_t perThreadScratchSpaceSlot0Size, NEO::LinearStream &commandStream, NEO::StreamProperties &streamProperties) override {
        return;
    }

    ze_result_t initialize(bool copyOnly, bool isInternal, bool immediateCmdListQueue) override {
        auto returnCode = BaseClass::initialize(copyOnly, isInternal, immediateCmdListQueue);

        if (this->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) {
            this->csr->createGlobalStatelessHeap();
        }
        return returnCode;
    }
};

using ExecuteCommandListTests = Test<DeviceFixture>;
HWTEST_F(ExecuteCommandListTests, givenExecuteCommandListWhenItReturnsThenContainersAreEmpty) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = new MockCommandQueue<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);
    auto commandList = new CommandListCoreFamily<FamilyType::gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    commandList->setCommandListPerThreadScratchSize(0u, 100u);
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, 1u /*num gmms*/, NEO::AllocationType::buffer, alloc, 0u, 0u, 1u, MemoryPool::system4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, 1u /*num gmms*/, NEO::AllocationType::buffer, alloc, 0u, 0u, 1u, MemoryPool::system4KBPages, 1u);

    commandList->commandContainer.getSshAllocations().push_back(&graphicsAllocation1);
    commandList->commandContainer.getSshAllocations().push_back(&graphicsAllocation2);

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

    EXPECT_EQ(0u, commandQueue->csr->getResidencyAllocations().size());
    EXPECT_EQ(0u, commandQueue->heapContainer.size());

    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

HWTEST_F(ExecuteCommandListTests, givenDebugFlagAndRegularCmdListWhenExecutionThenIncSubmissionCounter) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    DebugManagerStateRestore restore;

    debugManager.flags.EnableInOrderRegularCmdListPatching.set(1);
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = makeZeUniquePtr<MockCommandQueue<FamilyType::gfxCoreFamily>>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    {
        auto computeCmdList = makeZeUniquePtr<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
        computeCmdList->initialize(device, NEO::EngineGroupType::compute, 0u);
        computeCmdList->enableInOrderExecution();

        auto commandListHandle = computeCmdList->toHandle();
        computeCmdList->close();

        commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(1u, computeCmdList->inOrderExecInfo->getRegularCmdListSubmissionCounter());

        commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(2u, computeCmdList->inOrderExecInfo->getRegularCmdListSubmissionCounter());
    }

    {
        auto copyCmdList = makeZeUniquePtr<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
        copyCmdList->initialize(device, NEO::EngineGroupType::copy, 0u);
        copyCmdList->enableInOrderExecution();

        auto commandListHandle = copyCmdList->toHandle();
        copyCmdList->close();

        commandQueue->isCopyOnlyCommandQueue = true;
        commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(1u, copyCmdList->inOrderExecInfo->getRegularCmdListSubmissionCounter());

        commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(2u, copyCmdList->inOrderExecInfo->getRegularCmdListSubmissionCounter());
    }
}

HWTEST_F(ExecuteCommandListTests, givenRegularCmdListWhenExecutionThenDontIncSubmissionCounter) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = makeZeUniquePtr<MockCommandQueue<FamilyType::gfxCoreFamily>>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    {
        auto computeCmdList = makeZeUniquePtr<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
        computeCmdList->initialize(device, NEO::EngineGroupType::compute, 0u);
        computeCmdList->enableInOrderExecution();

        auto commandListHandle = computeCmdList->toHandle();
        computeCmdList->close();

        commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(0u, computeCmdList->inOrderExecInfo->getRegularCmdListSubmissionCounter());

        commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(0u, computeCmdList->inOrderExecInfo->getRegularCmdListSubmissionCounter());
    }

    {
        auto copyCmdList = makeZeUniquePtr<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
        copyCmdList->initialize(device, NEO::EngineGroupType::copy, 0u);
        copyCmdList->enableInOrderExecution();

        auto commandListHandle = copyCmdList->toHandle();
        copyCmdList->close();

        commandQueue->isCopyOnlyCommandQueue = true;
        commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(0u, copyCmdList->inOrderExecInfo->getRegularCmdListSubmissionCounter());

        commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
        EXPECT_EQ(0u, copyCmdList->inOrderExecInfo->getRegularCmdListSubmissionCounter());
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockCommandQueueSubmitBatchBuffer : public MockCommandQueue<gfxCoreFamily> {
  public:
    MockCommandQueueSubmitBatchBuffer(L0::Device *device, NEO::CommandStreamReceiver *csr, const ze_command_queue_desc_t *desc) : MockCommandQueue<gfxCoreFamily>(device, csr, desc) {}

    ADDMETHOD_NOBASE(submitBatchBuffer, NEO::SubmissionStatus, NEO::SubmissionStatus::success,
                     (size_t offset, NEO::ResidencyContainer &residencyContainer, void *endingCmdPtr,
                      bool isCooperative));
};

HWTEST_F(ExecuteCommandListTests, givenOutOfMemorySubmitBatchBufferThenExecuteCommandListReturnsOutOfMemoryError) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = new MockCommandQueueSubmitBatchBuffer<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->submitBatchBufferResult = NEO::SubmissionStatus::outOfMemory;

    commandQueue->initialize(false, false, false);
    auto commandList = new CommandListCoreFamily<FamilyType::gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    auto res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, res);

    commandQueue->destroy();
    commandList->destroy();
}

using CommandQueueDestroy = Test<DeviceFixture>;
HWTEST_F(CommandQueueDestroy, givenCommandQueueAndCommandListWithSshAndScratchWhenExecuteThenSshWasUsed) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = new MockCommandQueue<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);
    auto commandList = new CommandListCoreFamily<FamilyType::gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    commandList->setCommandListPerThreadScratchSize(0u, 100u);

    if (commandList->commandContainer.getIndirectHeap(NEO::HeapType::surfaceState) == nullptr) {
        commandList->commandContainer.prepareBindfulSsh();
    }

    auto commandListHandle = commandList->toHandle();
    commandList->close();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, 1u /*num gmms*/, NEO::AllocationType::buffer, alloc, 0u, 0u, 1u, MemoryPool::system4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, 1u /*num gmms*/, NEO::AllocationType::buffer, alloc, 0u, 0u, 1u, MemoryPool::system4KBPages, 1u);

    commandList->commandContainer.getSshAllocations().push_back(&graphicsAllocation1);
    commandList->commandContainer.getSshAllocations().push_back(&graphicsAllocation2);

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

    EXPECT_EQ(commandQueue->mockHeapContainer.size(), 3u);
    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

HWTEST2_F(CommandQueueDestroy, givenCommandQueueAndCommandListWithSshAndPrivateScratchWhenExecuteThenSshWasUsed, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = new MockCommandQueue<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);
    auto commandList = new CommandListCoreFamily<FamilyType::gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    commandList->setCommandListPerThreadScratchSize(1u, 100u);

    if (commandList->commandContainer.getIndirectHeap(NEO::HeapType::surfaceState) == nullptr) {
        commandList->commandContainer.prepareBindfulSsh();
    }

    auto commandListHandle = commandList->toHandle();
    commandList->close();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, 1u /*num gmms*/, NEO::AllocationType::buffer, alloc, 0u, 0u, 1u, MemoryPool::system4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, 1u /*num gmms*/, NEO::AllocationType::buffer, alloc, 0u, 0u, 1u, MemoryPool::system4KBPages, 1u);

    commandList->commandContainer.getSshAllocations().push_back(&graphicsAllocation1);
    commandList->commandContainer.getSshAllocations().push_back(&graphicsAllocation2);

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

    EXPECT_EQ(commandQueue->mockHeapContainer.size(), 3u);
    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

HWTEST_F(ExecuteCommandListTests, givenBindlessHelperWhenCommandListIsExecutedOnCommandQueueThenHeapContainerIsEmpty) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(1);
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    auto bindlessHeapsHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice, neoDevice->getNumGenericSubDevices() > 1);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapsHelper.release());
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = new MockCommandQueue<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);
    auto commandList = new CommandListCoreFamily<FamilyType::gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    commandList->setCommandListPerThreadScratchSize(0u, 100u);
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

    EXPECT_EQ(commandQueue->mockHeapContainer.size(), 0u);
    EXPECT_EQ(commandQueue->heapContainer.size(), 0u);

    commandQueue->destroy();
    commandList->destroy();
}

HWTEST_F(ExecuteCommandListTests, givenFailingSubmitBatchBufferThenExecuteCommandListReturnsErrorUnknown) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = new MockCommandQueueSubmitBatchBuffer<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->submitBatchBufferResult = NEO::SubmissionStatus::failed;

    commandQueue->initialize(false, false, false);
    auto commandList = new CommandListCoreFamily<FamilyType::gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    auto res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);

    commandQueue->destroy();
    commandList->destroy();
}

HWTEST_F(ExecuteCommandListTests, givenFailingSubmitBatchBufferThenResetGraphicsTaskCountsLatestFlushedTaskCountZero) {
    ze_command_queue_desc_t desc = {};

    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);

    auto commandQueue = new MockCommandQueueSubmitBatchBuffer<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->submitBatchBufferResult = NEO::SubmissionStatus::failed;

    commandQueue->initialize(false, false, false);
    auto commandList = new CommandListCoreFamily<FamilyType::gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, 1u /*num gmms*/, NEO::AllocationType::buffer, alloc, 0u, 0u, 1u, MemoryPool::system4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, 1u /*num gmms*/, NEO::AllocationType::buffer, alloc, 0u, 0u, 1u, MemoryPool::system4KBPages, 1u);
    commandList->commandContainer.addToResidencyContainer(&graphicsAllocation1);
    commandList->commandContainer.addToResidencyContainer(&graphicsAllocation2);
    static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(csr)->taskCount = 0;
    auto res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);

    EXPECT_EQ(NEO::GraphicsAllocation::objectNotUsed, graphicsAllocation1.getTaskCount(csr->getOsContext().getContextId()));
    EXPECT_EQ(NEO::GraphicsAllocation::objectNotUsed, graphicsAllocation2.getTaskCount(csr->getOsContext().getContextId()));

    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

HWTEST_F(ExecuteCommandListTests, givenFailingSubmitBatchBufferThenResetGraphicsTaskCountsLatestFlushedTaskCountNonZero) {
    ze_command_queue_desc_t desc = {};

    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);

    auto commandQueue = new MockCommandQueueSubmitBatchBuffer<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->submitBatchBufferResult = NEO::SubmissionStatus::failed;

    commandQueue->initialize(false, false, false);
    auto commandList = new CommandListCoreFamily<FamilyType::gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    void *alloc = alignedMalloc(0x100, 0x100);
    NEO::GraphicsAllocation graphicsAllocation1(0, 1u /*num gmms*/, NEO::AllocationType::buffer, alloc, 0u, 0u, 1u, MemoryPool::system4KBPages, 1u);
    NEO::GraphicsAllocation graphicsAllocation2(0, 1u /*num gmms*/, NEO::AllocationType::buffer, alloc, 0u, 0u, 1u, MemoryPool::system4KBPages, 1u);
    commandList->commandContainer.addToResidencyContainer(&graphicsAllocation1);
    commandList->commandContainer.addToResidencyContainer(&graphicsAllocation2);
    static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(csr)->taskCount = 2;
    auto res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);

    auto expectedTaskCount = 2u;
    EXPECT_EQ(expectedTaskCount, graphicsAllocation1.getTaskCount(csr->getOsContext().getContextId()));
    EXPECT_EQ(expectedTaskCount, graphicsAllocation2.getTaskCount(csr->getOsContext().getContextId()));
    commandQueue->destroy();
    commandList->destroy();
    alignedFree(alloc);
}

HWTEST_F(ExecuteCommandListTests, givenFailingSubmitBatchBufferThenWaitForCompletionFalse) {

    auto &compilerProductHelper = device->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    auto heaplessStateInitEnabled = compilerProductHelper.isHeaplessStateInitEnabled(heaplessEnabled);

    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = new MockCommandQueueSubmitBatchBuffer<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->submitBatchBufferResult = NEO::SubmissionStatus::failed;

    commandQueue->initialize(false, false, false);
    auto commandList = new CommandListCoreFamily<FamilyType::gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    TaskCountType flushedTaskCountPrior = csr->peekTaskCount();
    csr->setLatestFlushedTaskCount(flushedTaskCountPrior);
    auto res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_ERROR_UNKNOWN, res);

    auto expectedFlushedTaskCount = heaplessStateInitEnabled ? 1u : 0u;
    EXPECT_EQ(expectedFlushedTaskCount, csr->peekLatestFlushedTaskCount());

    commandQueue->destroy();
    commandList->destroy();
}

HWTEST_F(ExecuteCommandListTests, givenSuccessfulSubmitBatchBufferThenExecuteCommandListReturnsSuccess) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = new MockCommandQueueSubmitBatchBuffer<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->submitBatchBufferResult = NEO::SubmissionStatus::success;

    commandQueue->initialize(false, false, false);
    auto commandList = new CommandListCoreFamily<FamilyType::gfxCoreFamily>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    auto &ultCsr = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    ultCsr.recursiveLockCounter = 0;

    auto res = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    EXPECT_EQ(1u, ultCsr.recursiveLockCounter);

    commandQueue->destroy();
    commandList->destroy();
}

HWTEST_F(ExecuteCommandListTests, givenCommandQueueHavingTwoB2BCommandListsThenMVSDirtyFlagAndGSBADirtyFlagAreSetOnlyOnce) {

    auto &compilerProductHelper = device->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    if (heaplessEnabled) {
        GTEST_SKIP();
    }

    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    ze_result_t returnValue;
    auto commandQueue = CommandQueue::create(productFamily,
                                             device,
                                             csr,
                                             &desc,
                                             false,
                                             false,
                                             false,
                                             returnValue);
    auto commandList0 = new CommandListCoreFamily<FamilyType::gfxCoreFamily>();
    commandList0->initialize(device, NEO::EngineGroupType::compute, 0u);
    commandList0->setCommandListPerThreadScratchSize(0u, 0u);
    auto commandList1 = new CommandListCoreFamily<FamilyType::gfxCoreFamily>();
    commandList1->initialize(device, NEO::EngineGroupType::compute, 0u);
    commandList1->setCommandListPerThreadScratchSize(0u, 0u);
    auto commandListHandle0 = commandList0->toHandle();
    commandList0->close();
    auto commandListHandle1 = commandList1->toHandle();
    commandList1->close();

    EXPECT_EQ(true, csr->getMediaVFEStateDirty());
    EXPECT_EQ(true, csr->getGSBAStateDirty());
    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(false, csr->getMediaVFEStateDirty());
    EXPECT_EQ(false, csr->getGSBAStateDirty());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(false, csr->getMediaVFEStateDirty());
    EXPECT_EQ(false, csr->getGSBAStateDirty());

    commandQueue->destroy();
    commandList0->destroy();
    commandList1->destroy();
}

using CommandQueueExecuteSupport = IsGen12LP;
HWTEST2_F(ExecuteCommandListTests, givenCommandQueueHavingTwoB2BCommandListsThenMVSIsProgrammedOnlyOnce, CommandQueueExecuteSupport) {
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    commandList0->setCommandListPerThreadScratchSize(0u, 0u);
    auto commandList1 = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    commandList1->setCommandListPerThreadScratchSize(0u, 0u);
    auto commandListHandle0 = commandList0->toHandle();
    commandList0->close();
    auto commandListHandle1 = commandList1->toHandle();
    commandList1->close();

    ASSERT_NE(nullptr, commandQueue);

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false, nullptr, nullptr);
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false, nullptr, nullptr);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

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
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    auto commandList1 = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    commandList0->setCommandListPerThreadScratchSize(0u, 512u);
    commandList1->setCommandListPerThreadScratchSize(0u, 0u);
    auto commandListHandle0 = commandList0->toHandle();
    commandList0->close();
    auto commandListHandle1 = commandList1->toHandle();
    commandList1->close();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have only 1 state added
    ASSERT_EQ(1u, mediaVfeStates.size());
    ASSERT_EQ(1u, gsbaStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(0u, 0u);
    commandList0->close();
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(0u, 0u);
    commandList1->close();

    auto commandQueue1 = whiteboxCast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           false,
                                                           returnValue));

    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());

    usedSpaceAfter = commandQueue1->commandStream.getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream.getCpuBase(), 0), usedSpaceAfter));

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
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    auto commandList1 = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    commandList0->setCommandListPerThreadScratchSize(0u, 0u);
    commandList1->setCommandListPerThreadScratchSize(0u, 512u);
    auto commandListHandle0 = commandList0->toHandle();
    commandList0->close();
    auto commandListHandle1 = commandList1->toHandle();
    commandList1->close();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(0u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have 2 states added
    ASSERT_EQ(2u, mediaVfeStates.size());
    ASSERT_EQ(2u, gsbaStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(0u, 512u);
    commandList0->close();
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(0u, 0u);
    commandList1->close();

    auto commandQueue1 = whiteboxCast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           false,
                                                           returnValue));

    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());

    usedSpaceAfter = commandQueue1->commandStream.getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream.getCpuBase(), 0), usedSpaceAfter));

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
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    auto commandList1 = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    commandList0->setCommandListPerThreadScratchSize(0u, 512u);
    commandList1->setCommandListPerThreadScratchSize(0u, 512u);
    auto commandListHandle0 = commandList0->toHandle();
    commandList0->close();
    auto commandListHandle1 = commandList1->toHandle();
    commandList1->close();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have only 1 state added
    ASSERT_EQ(1u, mediaVfeStates.size());
    ASSERT_EQ(1u, gsbaStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(0u, 1024u);
    commandList0->close();
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(0u, 1024u);
    commandList1->close();

    auto commandQueue1 = whiteboxCast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           false,
                                                           returnValue));

    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(1024u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(1024u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());

    usedSpaceAfter = commandQueue1->commandStream.getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream.getCpuBase(), 0), usedSpaceAfter));

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
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    auto commandList1 = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    commandList0->setCommandListPerThreadScratchSize(0u, 0u);
    commandList1->setCommandListPerThreadScratchSize(0u, 512u);
    auto commandListHandle0 = commandList0->toHandle();
    commandList0->close();
    auto commandListHandle1 = commandList1->toHandle();
    commandList1->close();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(0u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    auto gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    // We should have 2 states added
    ASSERT_EQ(2u, mediaVfeStates.size());
    ASSERT_EQ(2u, gsbaStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(0u, 1024u);
    commandList0->close();
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(0u, 2048u);
    commandList1->close();

    auto commandQueue1 = whiteboxCast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           false,
                                                           returnValue));
    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(1024u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(2048u, csr->getScratchSpaceController()->getPerThreadScratchSpaceSizeSlot0());

    usedSpaceAfter = commandQueue1->commandStream.getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream.getCpuBase(), 0), usedSpaceAfter));

    mediaVfeStates = findAll<MEDIA_VFE_STATE *>(cmdList1.begin(), cmdList1.end());
    gsbaStates = findAll<STATE_BASE_ADDRESS *>(cmdList1.begin(), cmdList1.end());
    // We should have 2 states added
    ASSERT_EQ(2u, mediaVfeStates.size());
    ASSERT_EQ(2u, gsbaStates.size());

    commandQueue->destroy();
    commandQueue1->destroy();
}

HWTEST2_F(ExecuteCommandListTests, givenTwoCommandQueuesHavingTwoB2BCommandListsAndWithPrivateScratchUniquePerCmdListThenCFEIsProgrammedOncePerSubmission, IsHeapfulRequiredAndAtLeastXeCore) {

    auto &compilerProductHelper = device->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    if (heaplessEnabled) {
        GTEST_SKIP();
    }

    using CFE_STATE = typename FamilyType::CFE_STATE;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList0 = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    auto commandList1 = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    commandList0->setCommandListPerThreadScratchSize(1u, 0u);
    commandList1->setCommandListPerThreadScratchSize(1u, 512u);

    if (commandList0->commandContainer.getIndirectHeap(NEO::HeapType::surfaceState) == nullptr) {
        commandList0->commandContainer.prepareBindfulSsh();
    }
    if (commandList1->commandContainer.getIndirectHeap(NEO::HeapType::surfaceState) == nullptr) {
        commandList1->commandContainer.prepareBindfulSsh();
    }

    auto commandListHandle0 = commandList0->toHandle();
    commandList0->close();
    auto commandListHandle1 = commandList1->toHandle();
    commandList1->close();

    commandQueue->executeCommandLists(1, &commandListHandle0, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(0u, csr->getScratchSpaceController()->getPerThreadScratchSizeSlot1());
    commandQueue->executeCommandLists(1, &commandListHandle1, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(512u, csr->getScratchSpaceController()->getPerThreadScratchSizeSlot1());

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto mediaVfeStates = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());

    ASSERT_EQ(2u, mediaVfeStates.size());

    commandList0->reset();
    commandList0->setCommandListPerThreadScratchSize(1u, 1024u);
    commandList0->close();
    commandList1->reset();
    commandList1->setCommandListPerThreadScratchSize(1u, 2048u);
    commandList1->close();

    auto commandQueue1 = whiteboxCast(CommandQueue::create(productFamily,
                                                           device,
                                                           csr,
                                                           &desc,
                                                           false,
                                                           false,
                                                           false,
                                                           returnValue));
    commandQueue1->executeCommandLists(1, &commandListHandle0, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(1024u, csr->getScratchSpaceController()->getPerThreadScratchSizeSlot1());
    commandQueue1->executeCommandLists(1, &commandListHandle1, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(2048u, csr->getScratchSpaceController()->getPerThreadScratchSizeSlot1());

    usedSpaceAfter = commandQueue1->commandStream.getUsed();

    GenCmdList cmdList1;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList1, ptrOffset(commandQueue1->commandStream.getCpuBase(), 0), usedSpaceAfter));

    mediaVfeStates = findAll<CFE_STATE *>(cmdList1.begin(), cmdList1.end());

    ASSERT_EQ(2u, mediaVfeStates.size());

    commandQueue->destroy();
    commandQueue1->destroy();
}

HWTEST_F(ExecuteCommandListTests, givenDirectSubmissionEnabledWhenExecutingCmdListThenSetNonZeroBatchBufferStartAddress) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(csr)->directSubmissionAvailable = true;
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    commandList->setCommandListPerThreadScratchSize(1u, 0u);
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto bbStartCmds = findAll<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());

    auto heaplessStateInitEnabled = commandList->heaplessStateInitEnabled;
    EXPECT_EQ(heaplessStateInitEnabled ? 1u : 2u, bbStartCmds.size());

    for (auto &cmd : bbStartCmds) {
        auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*cmd);

        EXPECT_NE(0u, bbStart->getBatchBufferStartAddress());
    }

    static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(csr)->directSubmissionAvailable = false;
    commandQueue->destroy();
}

HWTEST_F(ExecuteCommandListTests, givenDirectSubmissionEnabledAndDebugFlagSetWhenExecutingCmdListThenSetZeroBatchBufferStartAddress) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.BatchBufferStartPrepatchingWaEnabled.set(0);

    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(csr)->directSubmissionAvailable = true;
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          csr,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    commandList->setCommandListPerThreadScratchSize(1u, 0u);
    auto commandListHandle = commandList->toHandle();
    commandList->close();

    commandQueue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    auto bbStartCmds = findAll<MI_BATCH_BUFFER_START *>(cmdList.begin(), cmdList.end());

    auto heaplessStateInitEnabled = commandList->heaplessStateInitEnabled;
    EXPECT_EQ(heaplessStateInitEnabled ? 1u : 2u, bbStartCmds.size());

    for (auto &cmd : bbStartCmds) {
        auto bbStart = genCmdCast<MI_BATCH_BUFFER_START *>(*cmd);
        if (cmd == bbStartCmds.back()) {
            EXPECT_EQ(0u, bbStart->getBatchBufferStartAddress());
        } else {
            EXPECT_NE(0u, bbStart->getBatchBufferStartAddress());
        }
    }

    static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(csr)->directSubmissionAvailable = false;
    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenOverrideCmdQueueSyncModeToDefaultWhenCommandQueueIsCreatedWithSynchronousModeThenDefaultModeIsSelected) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.OverrideCmdQueueSynchronousMode.set(0);

    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_ERROR_DEVICE_LOST;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    auto cmdQueueSynchronousMode = reinterpret_cast<L0::CommandQueueImp *>(commandQueue)->getCommandQueueMode();
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_DEFAULT, cmdQueueSynchronousMode);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenOverrideCmdQueueSyncModeToAsynchronousWhenCommandQueueIsCreatedWithSynchronousModeThenAsynchronousModeIsSelected) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.OverrideCmdQueueSynchronousMode.set(2);

    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_ERROR_DEVICE_LOST;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    auto cmdQueueSynchronousMode = reinterpret_cast<L0::CommandQueueImp *>(commandQueue)->getCommandQueueMode();
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, cmdQueueSynchronousMode);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenOverrideCmdQueueSyncModeToSynchronousWhenCommandQueueIsCreatedWithAsynchronousModeThenSynchronousModeIsSelected) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.OverrideCmdQueueSynchronousMode.set(1);

    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_ERROR_DEVICE_LOST;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    auto cmdQueueSynchronousMode = reinterpret_cast<L0::CommandQueueImp *>(commandQueue)->getCommandQueueMode();
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, cmdQueueSynchronousMode);

    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCreatedCommandQueueWhenGettingTrackingFlagsThenDefaultValuseIsHwSupported) {
    auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();

    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    auto &productHelper = rootDeviceEnvironment.getHelper<NEO::ProductHelper>();

    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));

    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    ASSERT_NE(nullptr, commandQueue);

    bool expectedStateComputeModeTracking = l0GfxCoreHelper.platformSupportsStateComputeModeTracking();
    EXPECT_EQ(expectedStateComputeModeTracking, commandQueue->stateComputeModeTracking);

    bool expectedPipelineSelectTracking = l0GfxCoreHelper.platformSupportsPipelineSelectTracking();
    EXPECT_EQ(expectedPipelineSelectTracking, commandQueue->pipelineSelectStateTracking);

    bool expectedFrontEndTracking = l0GfxCoreHelper.platformSupportsFrontEndTracking();
    EXPECT_EQ(expectedFrontEndTracking, commandQueue->frontEndStateTracking);

    bool expectedStateBaseAddressTracking = l0GfxCoreHelper.platformSupportsStateBaseAddressTracking(device->getNEODevice()->getRootDeviceEnvironment());
    EXPECT_EQ(expectedStateBaseAddressTracking, commandQueue->stateBaseAddressTracking);

    bool expectedDoubleSbaWa = productHelper.isAdditionalStateBaseAddressWARequired(device->getHwInfo());
    EXPECT_EQ(expectedDoubleSbaWa, commandQueue->doubleSbaWa);

    auto expectedHeapAddressModel = l0GfxCoreHelper.getPlatformHeapAddressModel(device->getNEODevice()->getRootDeviceEnvironment());
    EXPECT_EQ(expectedHeapAddressModel, commandQueue->cmdListHeapAddressModel);

    auto expectedDispatchCmdListBatchBufferAsPrimary = L0GfxCoreHelper::dispatchCmdListBatchBufferAsPrimary(rootDeviceEnvironment, true);
    EXPECT_EQ(expectedDispatchCmdListBatchBufferAsPrimary, commandQueue->dispatchCmdListBatchBufferAsPrimary);

    commandQueue->destroy();
}

struct SVMAllocsManagerMock : public NEO::SVMAllocsManager {
    using SVMAllocsManager::mtxForIndirectAccess;
    SVMAllocsManagerMock(MemoryManager *memoryManager) : NEO::SVMAllocsManager(memoryManager) {}
    void makeIndirectAllocationsResident(CommandStreamReceiver &commandStreamReceiver, TaskCountType taskCount) override {
        makeIndirectAllocationsResidentCalledTimes++;
    }
    void addInternalAllocationsToResidencyContainer(uint32_t rootDeviceIndex,
                                                    ResidencyContainer &residencyContainer,
                                                    uint32_t requestedTypesMask) override {
        addInternalAllocationsToResidencyContainerCalledTimes++;
        passedContainer = residencyContainer.data();
    }
    uint32_t makeIndirectAllocationsResidentCalledTimes = 0;
    uint32_t addInternalAllocationsToResidencyContainerCalledTimes = 0;
    GraphicsAllocation **passedContainer;
};

TEST_F(CommandQueueCreate, givenCommandQueueWhenHandleIndirectAllocationResidencyCalledAndSubmiPackEnabledThenMakeIndirectAllocResidentCalled) {
    DebugManagerStateRestore restore;
    debugManager.flags.MakeIndirectAllocationsResidentAsPack.set(1);
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto prevSvmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    std::unique_lock<std::mutex> lock;
    auto mockSvmAllocsManager = std::make_unique<SVMAllocsManagerMock>(device->getDriverHandle()->getMemoryManager());
    reinterpret_cast<WhiteBox<::L0::DriverHandleImp> *>(device->getDriverHandle())->svmAllocsManager = mockSvmAllocsManager.get();

    commandQueue->handleIndirectAllocationResidency({true, true, true}, lock, false);
    EXPECT_EQ(mockSvmAllocsManager->makeIndirectAllocationsResidentCalledTimes, 1u);
    EXPECT_EQ(mockSvmAllocsManager->addInternalAllocationsToResidencyContainerCalledTimes, 0u);
    reinterpret_cast<WhiteBox<::L0::DriverHandleImp> *>(device->getDriverHandle())->svmAllocsManager = prevSvmAllocsManager;
    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCommandQueueWhenHandleIndirectAllocationResidencyCalledAndSubmiPackDisabeldThenAddInternalAllocationsToResidencyContainer) {
    DebugManagerStateRestore restore;
    debugManager.flags.MakeIndirectAllocationsResidentAsPack.set(0);
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto prevSvmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    std::unique_lock<std::mutex> lock;
    auto mockSvmAllocsManager = std::make_unique<SVMAllocsManagerMock>(device->getDriverHandle()->getMemoryManager());
    reinterpret_cast<WhiteBox<::L0::DriverHandleImp> *>(device->getDriverHandle())->svmAllocsManager = mockSvmAllocsManager.get();

    commandQueue->handleIndirectAllocationResidency({true, true, true}, lock, false);
    EXPECT_EQ(mockSvmAllocsManager->makeIndirectAllocationsResidentCalledTimes, 0u);
    EXPECT_EQ(mockSvmAllocsManager->addInternalAllocationsToResidencyContainerCalledTimes, 1u);
    reinterpret_cast<WhiteBox<::L0::DriverHandleImp> *>(device->getDriverHandle())->svmAllocsManager = prevSvmAllocsManager;
    lock.unlock();
    commandQueue->destroy();
}

TEST_F(CommandQueueCreate, givenCommandQueueWhenHandleIndirectAllocationResidencyCalledAndSubmiPackDisabeldThenSVMAllocsMtxIsLocked) {
    DebugManagerStateRestore restore;
    debugManager.flags.MakeIndirectAllocationsResidentAsPack.set(0);
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto prevSvmAllocsManager = device->getDriverHandle()->getSvmAllocsManager();
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    std::unique_lock<std::mutex> lock;
    auto mockSvmAllocsManager = std::make_unique<SVMAllocsManagerMock>(device->getDriverHandle()->getMemoryManager());
    reinterpret_cast<WhiteBox<::L0::DriverHandleImp> *>(device->getDriverHandle())->svmAllocsManager = mockSvmAllocsManager.get();

    commandQueue->handleIndirectAllocationResidency({true, true, true}, lock, false);
    std::thread th([&] {
        EXPECT_FALSE(mockSvmAllocsManager->mtxForIndirectAccess.try_lock());
    });
    th.join();
    reinterpret_cast<WhiteBox<::L0::DriverHandleImp> *>(device->getDriverHandle())->svmAllocsManager = prevSvmAllocsManager;
    lock.unlock();
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest, givenDeviceWhenCreateCommandQueueForInternalUsageThenInternalEngineCsrUsed) {
    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;

    ze_command_queue_handle_t commandQueueHandle = {};

    auto internalEngine = neoDevice->getInternalEngine();
    auto internalCsr = internalEngine.commandStreamReceiver;

    reinterpret_cast<L0::DeviceImp *>(device)->createInternalCommandQueue(&desc, &commandQueueHandle);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_EQ(commandQueue->getCsr(), internalCsr);
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest, givenDeviceWhenCreateCommandQueueForNonInternalUsageThenInternalEngineCsrNotUsed) {
    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;

    ze_command_queue_handle_t commandQueueHandle = {};

    auto internalEngine = neoDevice->getInternalEngine();
    auto internalCsr = internalEngine.commandStreamReceiver;

    device->createCommandQueue(&desc, &commandQueueHandle);
    auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_NE(commandQueue->getCsr(), internalCsr);
    commandQueue->destroy();
}

TEST_F(DeviceCreateCommandQueueTest, givenDeviceWhenCreateCommandQueueForValidOrdinalAndIndexThenGetOrdinalReturnsCorrectOrdinal) {
    auto &engineGroups = neoDevice->getRegularEngineGroups();
    auto internalEngine = neoDevice->getInternalEngine();
    auto internalCsr = internalEngine.commandStreamReceiver;
    for (uint32_t ordinal = 0; ordinal < engineGroups.size(); ordinal++) {
        for (uint32_t index = 0; index < engineGroups[ordinal].engines.size(); index++) {
            ze_command_queue_desc_t desc{};
            desc.ordinal = ordinal;
            desc.index = index;

            ze_command_queue_handle_t commandQueueHandle = {};

            EXPECT_EQ(ZE_RESULT_SUCCESS, device->createCommandQueue(&desc, &commandQueueHandle));
            auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
            EXPECT_NE(commandQueue->getCsr(), internalCsr);

            uint32_t commandQueueOrdinal = 0u;
            EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->getOrdinal(&commandQueueOrdinal));
            EXPECT_EQ(desc.ordinal, commandQueueOrdinal);

            commandQueue->destroy();
        }
    }
}

TEST_F(DeviceCreateCommandQueueTest, givenDeviceWhenCreateCommandQueueForValidOrdinalAndIndexThenGetIndexReturnsCorrectIndex) {
    auto &engineGroups = neoDevice->getRegularEngineGroups();
    auto internalEngine = neoDevice->getInternalEngine();
    auto internalCsr = internalEngine.commandStreamReceiver;
    for (uint32_t ordinal = 0; ordinal < engineGroups.size(); ordinal++) {
        for (uint32_t index = 0; index < engineGroups[ordinal].engines.size(); index++) {
            ze_command_queue_desc_t desc{};
            desc.ordinal = ordinal;
            desc.index = index;

            ze_command_queue_handle_t commandQueueHandle = {};

            EXPECT_EQ(ZE_RESULT_SUCCESS, device->createCommandQueue(&desc, &commandQueueHandle));
            auto commandQueue = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
            EXPECT_NE(commandQueue->getCsr(), internalCsr);

            uint32_t commandQueueIndex = 0u;
            EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->getIndex(&commandQueueIndex));
            EXPECT_EQ(desc.index, commandQueueIndex);

            commandQueue->destroy();
        }
    }
}

TEST_F(DeviceCreateCommandQueueTest, givenMakeEachEnqueueBlockingSetToOneWhenIsSynchronousModeIsCalledThenReturnsTrue) {
    DebugManagerStateRestore restore;
    debugManager.flags.MakeEachEnqueueBlocking.set(1);

    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    ze_command_queue_handle_t commandQueueHandle = {};

    device->createCommandQueue(&desc, &commandQueueHandle);
    auto commandQueueImp = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_TRUE(commandQueueImp->isSynchronousMode());
    commandQueueImp->destroy();
}

TEST_F(DeviceCreateCommandQueueTest, givenMakeEachEnqueueBlockingSetToZeroWhenIsSynchronousModeIsCalledThenReturnsFalse) {
    DebugManagerStateRestore restore;
    debugManager.flags.MakeEachEnqueueBlocking.set(0);

    ze_command_queue_desc_t desc{};
    desc.ordinal = 0u;
    desc.index = 0u;
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    ze_command_queue_handle_t commandQueueHandle = {};

    device->createCommandQueue(&desc, &commandQueueHandle);
    auto commandQueueImp = static_cast<CommandQueueImp *>(L0::CommandQueue::fromHandle(commandQueueHandle));
    EXPECT_FALSE(commandQueueImp->isSynchronousMode());
    commandQueueImp->destroy();
}

} // namespace ult
} // namespace L0
