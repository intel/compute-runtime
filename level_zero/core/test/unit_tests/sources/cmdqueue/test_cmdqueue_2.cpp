/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_xehp_and_later.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_memory_operations_handler.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include "level_zero/core/test/unit_tests/fixtures/aub_csr_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

namespace L0 {
template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListCoreFamily;
} // namespace L0
namespace NEO {
class InternalAllocationStorage;
class ScratchSpaceController;
enum QueueThrottle : uint32_t;
template <typename GfxFamily>
class UltAubCommandStreamReceiver;
} // namespace NEO

namespace L0 {
namespace ult {

using ContextCreateCommandQueueTest = Test<DeviceFixture>;

TEST_F(ContextCreateCommandQueueTest, givenCallToContextCreateCommandQueueThenCallSucceeds) {
    ze_command_queue_desc_t desc = {};
    desc.ordinal = 0u;

    ze_command_queue_handle_t commandQueue = {};

    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, commandQueue);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST_F(ContextCreateCommandQueueTest, givenEveryPossibleGroupIndexWhenCreatingCommandQueueThenCommandQueueIsCreated) {
    ze_command_queue_handle_t commandQueue = {};
    auto &engineGroups = neoDevice->getRegularEngineGroups();
    for (uint32_t ordinal = 0; ordinal < engineGroups.size(); ordinal++) {
        for (uint32_t index = 0; index < engineGroups[ordinal].engines.size(); index++) {
            ze_command_queue_desc_t desc = {};
            desc.ordinal = ordinal;
            desc.index = index;
            ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
            EXPECT_EQ(ZE_RESULT_SUCCESS, res);
            EXPECT_NE(nullptr, commandQueue);

            L0::CommandQueue::fromHandle(commandQueue)->destroy();
        }
    }
}

HWTEST_F(ContextCreateCommandQueueTest, givenOrdinalBiggerThanAvailableEnginesWhenCreatingCommandQueueThenInvalidArgumentErrorIsReturned) {
    ze_command_queue_handle_t commandQueue = {};
    auto &engineGroups = neoDevice->getRegularEngineGroups();
    ze_command_queue_desc_t desc = {};
    desc.ordinal = static_cast<uint32_t>(engineGroups.size());
    desc.index = 0;
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
    EXPECT_EQ(nullptr, commandQueue);

    desc.ordinal = 0;
    desc.index = 0x1000;
    res = context->createCommandQueue(device, &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
    EXPECT_EQ(nullptr, commandQueue);
}

HWTEST_F(ContextCreateCommandQueueTest, givenRootDeviceAndImplicitScalingDisabledWhenCreatingCommandQueueThenValidateQueueOrdinalUsingSubDeviceEngines) {
    NEO::UltDeviceFactory deviceFactory{1, 2};
    auto &rootDevice = *deviceFactory.rootDevices[0];
    auto &subDevice0 = *deviceFactory.subDevices[0];
    rootDevice.regularEngineGroups.resize(1);
    subDevice0.getRegularEngineGroups().push_back(NEO::EngineGroupT{});
    subDevice0.getRegularEngineGroups().back().engineGroupType = EngineGroupType::compute;
    subDevice0.getRegularEngineGroups().back().engines.resize(1);
    subDevice0.getRegularEngineGroups().back().engines[0].commandStreamReceiver = &rootDevice.getGpgpuCommandStreamReceiver();
    auto ordinal = static_cast<uint32_t>(subDevice0.getRegularEngineGroups().size() - 1);
    MockDeviceImp l0RootDevice(&rootDevice);

    l0RootDevice.driverHandle = driverHandle.get();

    ze_command_queue_handle_t commandQueue = nullptr;
    ze_command_queue_desc_t desc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    desc.ordinal = ordinal;
    desc.index = 0;

    l0RootDevice.implicitScalingCapable = true;
    ze_result_t res = context->createCommandQueue(l0RootDevice.toHandle(), &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
    EXPECT_EQ(nullptr, commandQueue);

    l0RootDevice.implicitScalingCapable = false;
    res = context->createCommandQueue(l0RootDevice.toHandle(), &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, commandQueue);
    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

using AubCsrTest = Test<AubCsrFixture>;

HWTEST_TEMPLATED_F(AubCsrTest, givenAubCsrSyncQueueAndKmdWaitWhenCallingExecuteCommandListsThenPollForCompletionIsCalled) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.OverrideUseKmdWaitFunction.set(1);

    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_command_queue_handle_t commandQueue = {};
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, commandQueue);

    auto aubCsr = static_cast<NEO::UltAubCommandStreamReceiver<FamilyType> *>(csr);
    CommandQueue *queue = static_cast<CommandQueue *>(L0::CommandQueue::fromHandle(commandQueue));
    EXPECT_EQ(aubCsr->pollForCompletionCalled, 0u);

    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    auto commandListHandle = commandList->toHandle();
    commandList->close();

    queue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(aubCsr->pollForCompletionCalled, 1u);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST_TEMPLATED_F(AubCsrTest, givenAubCsrAndAsyncQueueWhenCallingExecuteCommandListsThenPollForCompletionIsNotCalled) {
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_command_queue_handle_t commandQueue = {};
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, commandQueue);

    auto aubCsr = static_cast<NEO::UltAubCommandStreamReceiver<FamilyType> *>(csr);
    CommandQueue *queue = static_cast<CommandQueue *>(L0::CommandQueue::fromHandle(commandQueue));
    EXPECT_EQ(aubCsr->pollForCompletionCalled, 0u);

    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    auto commandListHandle = commandList->toHandle();
    commandList->close();

    queue->executeCommandLists(1, &commandListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(aubCsr->pollForCompletionCalled, 0u);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

using CommandQueueSynchronizeTest = Test<DeviceFixture>;
using MultiTileCommandQueueSynchronizeTest = Test<SingleRootMultiSubDeviceFixture>;

template <typename GfxFamily>
struct SynchronizeCsr : public NEO::UltCommandStreamReceiver<GfxFamily> {
    SynchronizeCsr(const NEO::ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
        : NEO::UltCommandStreamReceiver<GfxFamily>(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment), 0, deviceBitfield) {
        CommandStreamReceiver::tagAddress = &tagAddressData[0];
        memset(const_cast<TagAddressType *>(CommandStreamReceiver::tagAddress), 0xFFFFFFFF, tagSize * sizeof(uint32_t));
    }

    WaitStatus waitForCompletionWithTimeout(const WaitParams &params, TaskCountType taskCountToWait) override {
        enableTimeoutSet = params.enableTimeout;
        waitForComplitionCalledTimes++;
        partitionCountSet = this->activePartitions;
        return waitForCompletionWithTimeoutResult;
    }

    WaitStatus waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool quickKmdSleep, NEO::QueueThrottle throttle) override {
        waitForTaskCountWithKmdNotifyFallbackCalled++;
        return NEO::UltCommandStreamReceiver<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, quickKmdSleep, throttle);
    }

    static constexpr size_t tagSize = 128;
    static volatile TagAddressType tagAddressData[tagSize];
    uint32_t waitForComplitionCalledTimes = 0;
    uint32_t waitForTaskCountWithKmdNotifyFallbackCalled = 0;
    uint32_t partitionCountSet = 0;
    bool enableTimeoutSet = false;
    WaitStatus waitForCompletionWithTimeoutResult = WaitStatus::ready;
};

template <typename GfxFamily>
volatile TagAddressType SynchronizeCsr<GfxFamily>::tagAddressData[SynchronizeCsr<GfxFamily>::tagSize];

HWTEST_F(CommandQueueSynchronizeTest, givenCallToSynchronizeThenCorrectEnableTimeoutAndTimeoutValuesAreUsed) {
    auto csr = std::unique_ptr<SynchronizeCsr<FamilyType>>(new SynchronizeCsr<FamilyType>(*device->getNEODevice()->getExecutionEnvironment(),
                                                                                          device->getNEODevice()->getDeviceBitfield()));

    ze_command_queue_desc_t desc = {};
    ze_command_queue_handle_t commandQueue = {};
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, commandQueue);

    CommandQueue *queue = reinterpret_cast<CommandQueue *>(L0::CommandQueue::fromHandle(commandQueue));
    queue->csr = csr.get();

    uint64_t timeout = 10;

    queue->synchronize(timeout);

    EXPECT_EQ(1u, csr->waitForComplitionCalledTimes);
    EXPECT_EQ(0u, csr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_TRUE(csr->enableTimeoutSet);

    timeout = std::numeric_limits<uint64_t>::max();

    queue->synchronize(timeout);

    EXPECT_EQ(2u, csr->waitForComplitionCalledTimes);
    EXPECT_EQ(0u, csr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_FALSE(csr->enableTimeoutSet);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST_F(CommandQueueSynchronizeTest, givenGpuHangWhenCallingSynchronizeThenErrorIsPropagated) {
    auto csr = std::unique_ptr<SynchronizeCsr<FamilyType>>(new SynchronizeCsr<FamilyType>(*device->getNEODevice()->getExecutionEnvironment(),
                                                                                          device->getNEODevice()->getDeviceBitfield()));
    csr->waitForCompletionWithTimeoutResult = NEO::WaitStatus::gpuHang;

    ze_command_queue_desc_t desc{};
    ze_command_queue_handle_t commandQueue{};
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);

    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, commandQueue);

    auto queue = whiteboxCast(L0::CommandQueue::fromHandle(commandQueue));
    queue->csr = csr.get();

    constexpr auto timeout{std::numeric_limits<uint64_t>::max()};
    const auto synchronizationResult{queue->synchronize(timeout)};

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, synchronizationResult);
    EXPECT_EQ(1u, csr->waitForComplitionCalledTimes);
    EXPECT_EQ(0u, csr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_FALSE(csr->enableTimeoutSet);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST_F(CommandQueueSynchronizeTest, givenDebugOverrideEnabledAndGpuHangWhenCallingSynchronizeThenErrorIsPropagated) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.OverrideUseKmdWaitFunction.set(1);

    auto csr = std::unique_ptr<SynchronizeCsr<FamilyType>>(new SynchronizeCsr<FamilyType>(*device->getNEODevice()->getExecutionEnvironment(),
                                                                                          device->getNEODevice()->getDeviceBitfield()));
    csr->waitForCompletionWithTimeoutResult = NEO::WaitStatus::gpuHang;

    ze_command_queue_desc_t desc{};
    ze_command_queue_handle_t commandQueue{};
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);

    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, commandQueue);

    auto queue = whiteboxCast(L0::CommandQueue::fromHandle(commandQueue));
    queue->csr = csr.get();

    constexpr auto timeout{std::numeric_limits<uint64_t>::max()};
    const auto synchronizationResult{queue->synchronize(timeout)};

    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, synchronizationResult);
    EXPECT_EQ(1u, csr->waitForComplitionCalledTimes);
    EXPECT_EQ(1u, csr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_FALSE(csr->enableTimeoutSet);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST_F(CommandQueueSynchronizeTest, givenDebugOverrideEnabledWhenCallToSynchronizeThenCorrectEnableTimeoutAndTimeoutValuesAreUsed) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.OverrideUseKmdWaitFunction.set(1);

    auto csr = std::unique_ptr<SynchronizeCsr<FamilyType>>(new SynchronizeCsr<FamilyType>(*device->getNEODevice()->getExecutionEnvironment(),
                                                                                          device->getNEODevice()->getDeviceBitfield()));

    ze_command_queue_desc_t desc = {};
    ze_command_queue_handle_t commandQueue = {};
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, commandQueue);

    CommandQueue *queue = reinterpret_cast<CommandQueue *>(L0::CommandQueue::fromHandle(commandQueue));
    queue->csr = csr.get();

    uint64_t timeout = 10;

    queue->synchronize(timeout);

    EXPECT_EQ(1u, csr->waitForComplitionCalledTimes);
    EXPECT_EQ(0u, csr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_TRUE(csr->enableTimeoutSet);

    timeout = std::numeric_limits<uint64_t>::max();

    queue->synchronize(timeout);

    EXPECT_EQ(2u, csr->waitForComplitionCalledTimes);
    EXPECT_EQ(1u, csr->waitForTaskCountWithKmdNotifyFallbackCalled);
    EXPECT_FALSE(csr->enableTimeoutSet);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST2_F(MultiTileCommandQueueSynchronizeTest, givenMultiplePartitionCountWhenCallingSynchronizeThenExpectTheSameNumberCsrSynchronizeCalls, IsAtLeastXeCore) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto csr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);
    EXPECT_NE(0u, csr->getImmWritePostSyncWriteOffset());
    volatile TagAddressType *tagAddress = csr->getTagAddress();
    for (uint32_t i = 0; i < 2; i++) {
        *tagAddress = 0xFF;
        tagAddress = ptrOffset(tagAddress, csr->getImmWritePostSyncWriteOffset());
    }
    csr->activePartitions = 2u;
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
    EXPECT_EQ(2u, commandQueue->activeSubDevices);

    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);
    commandList->partitionCount = 2;

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    commandList->close();

    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    uint64_t timeout = std::numeric_limits<uint64_t>::max();
    commandQueue->synchronize(timeout);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST2_F(MultiTileCommandQueueSynchronizeTest, givenCsrHasMultipleActivePartitionWhenExecutingCmdListOnNewCmdQueueThenExpectCmdPartitionCountMatchCsrActivePartitions, IsAtLeastXeCore) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto csr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);

    EXPECT_NE(0u, csr->getImmWritePostSyncWriteOffset());
    volatile TagAddressType *tagAddress = csr->getTagAddress();
    for (uint32_t i = 0; i < 2; i++) {
        *tagAddress = 0xFF;
        tagAddress = ptrOffset(tagAddress, csr->getImmWritePostSyncWriteOffset());
    }
    csr->activePartitions = 2u;
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
    EXPECT_EQ(2u, commandQueue->activeSubDevices);

    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    commandList->close();

    commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    EXPECT_EQ(2u, commandQueue->partitionCount);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

template <typename GfxFamily>
struct TestCmdQueueCsr : public NEO::UltCommandStreamReceiver<GfxFamily> {
    TestCmdQueueCsr(const NEO::ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
        : NEO::UltCommandStreamReceiver<GfxFamily>(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment), 0, deviceBitfield) {
    }

    ADDMETHOD_NOBASE(waitForCompletionWithTimeout, NEO::WaitStatus, NEO::WaitStatus::notReady, (const WaitParams &params, TaskCountType taskCountToWait));
};

HWTEST_F(CommandQueueSynchronizeTest, givenSinglePartitionCountWhenWaitFunctionFailsThenReturnNotReady) {
    auto csr = std::unique_ptr<TestCmdQueueCsr<FamilyType>>(new TestCmdQueueCsr<FamilyType>(*device->getNEODevice()->getExecutionEnvironment(),
                                                                                            device->getNEODevice()->getDeviceBitfield()));
    csr->setupContext(*device->getNEODevice()->getDefaultEngine().osContext);

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

    uint64_t timeout = std::numeric_limits<uint64_t>::max();
    returnValue = commandQueue->synchronize(timeout);
    EXPECT_EQ(returnValue, ZE_RESULT_NOT_READY);

    commandQueue->destroy();
    EXPECT_EQ(1u, csr->waitForCompletionWithTimeoutCalled);
}

HWTEST_F(CommandQueueSynchronizeTest, givenSynchronousCommandQueueWhenTagUpdateFromWaitEnabledAndNoFenceUsedThenExpectPostSyncDispatch) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;
    using Parse = typename FamilyType::Parse;

    DebugManagerStateRestore restore;
    NEO::debugManager.flags.UpdateTaskCountFromWait.set(3);
    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);

    size_t expectedSize = sizeof(MI_BATCH_BUFFER_START);
    if (neoDevice->getDefaultEngine().commandStreamReceiver->isAnyDirectSubmissionEnabled()) {
        expectedSize += sizeof(MI_BATCH_BUFFER_START);
    } else {
        expectedSize += sizeof(MI_BATCH_BUFFER_END);
    }
    expectedSize += NEO::MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(neoDevice->getRootDeviceEnvironment(), NEO::PostSyncMode::immediateData);
    expectedSize = alignUp(expectedSize, 8);

    const ze_command_queue_desc_t desc{ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, nullptr, 0, 0, 0, ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, ZE_COMMAND_QUEUE_PRIORITY_NORMAL};
    ze_result_t returnValue;

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, commandQueue->getCommandQueueMode());

    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList);

    // 1st execute provides all preamble commands
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    commandList->close();

    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    size_t executionConsumedSize = usedSpaceAfter - usedSpaceBefore;

    EXPECT_EQ(expectedSize, executionConsumedSize);

    GenCmdList cmdList;
    ASSERT_TRUE(Parse::parseCommandBuffer(cmdList,
                                          ptrOffset(commandQueue->commandStream.getCpuBase(), usedSpaceBefore),
                                          executionConsumedSize));

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    size_t pipeControlsPostSyncNumber = 0u;
    TaskCountType expectedData = commandQueue->getCsr()->peekTaskCount();
    for (size_t i = 0; i < pipeControls.size(); i++) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*pipeControls[i]);
        if (pipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(commandQueue->getCsr()->getTagAllocation()->getGpuAddress(), NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*pipeControl));
            EXPECT_EQ(expectedData, pipeControl->getImmediateData());
            pipeControlsPostSyncNumber++;
        }
    }
    EXPECT_EQ(1u, pipeControlsPostSyncNumber);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

using CommandQueuePowerHintTest = Test<DeviceFixture>;

HWTEST_F(CommandQueuePowerHintTest, givenDriverHandleWithPowerHintAndOsContextPowerHintUnsetThenSuccessIsReturned) {
    auto csr = std::unique_ptr<TestCmdQueueCsr<FamilyType>>(new TestCmdQueueCsr<FamilyType>(*device->getNEODevice()->getExecutionEnvironment(),
                                                                                            device->getNEODevice()->getDeviceBitfield()));
    csr->setupContext(*device->getNEODevice()->getDefaultEngine().osContext);
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(device->getDriverHandle());
    driverHandleImp->powerHint = 1;

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
    commandQueue->destroy();
}

HWTEST_F(CommandQueuePowerHintTest, givenDriverHandleWithPowerHintAndOsContextPowerHintAlreadySetThenSuccessIsReturned) {
    auto csr = std::unique_ptr<TestCmdQueueCsr<FamilyType>>(new TestCmdQueueCsr<FamilyType>(*device->getNEODevice()->getExecutionEnvironment(),
                                                                                            device->getNEODevice()->getDeviceBitfield()));
    csr->setupContext(*device->getNEODevice()->getDefaultEngine().osContext);
    DriverHandleImp *driverHandleImp = static_cast<DriverHandleImp *>(device->getDriverHandle());
    driverHandleImp->powerHint = 1;
    auto &osContext = csr->getOsContext();
    osContext.setUmdPowerHintValue(1);

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
    commandQueue->destroy();
}

struct MemoryManagerCommandQueueCreateNegativeTest : public NEO::MockMemoryManager {
    MemoryManagerCommandQueueCreateNegativeTest(NEO::ExecutionEnvironment &executionEnvironment) : NEO::MockMemoryManager(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment)) {}
    NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const NEO::AllocationProperties &properties) override {
        if (forceFailureInPrimaryAllocation) {
            return nullptr;
        }
        return NEO::MemoryManager::allocateGraphicsMemoryWithProperties(properties);
    }
    bool forceFailureInPrimaryAllocation = false;
};

struct CommandQueueCreateNegativeTest : public ::testing::Test {
    void SetUp() override {
        executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        for (uint32_t i = 0; i < numRootDevices; i++) {
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
            executionEnvironment->rootDeviceEnvironments[i]->initGmm();
        }

        memoryManager = new MemoryManagerCommandQueueCreateNegativeTest(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        std::vector<std::unique_ptr<NEO::Device>> devices;
        for (uint32_t i = 0; i < numRootDevices; i++) {
            neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, i);
            devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        }

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        device = driverHandle->devices[0];
    }
    void TearDown() override {
    }

    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    MemoryManagerCommandQueueCreateNegativeTest *memoryManager = nullptr;
    const uint32_t numRootDevices = 1u;
};

TEST_F(CommandQueueCreateNegativeTest, whenDeviceAllocationFailsDuringCommandQueueCreateThenAppropriateValueIsReturned) {
    const ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);
    memoryManager->forceFailureInPrimaryAllocation = true;

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily,
                                                          device,
                                                          csr.get(),
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          returnValue);
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, returnValue);
    ASSERT_EQ(nullptr, commandQueue);
}

struct CommandQueueInitTests : public ::testing::Test {
    class MyMemoryManager : public OsAgnosticMemoryManager {
      public:
        using OsAgnosticMemoryManager::OsAgnosticMemoryManager;

        NEO::GraphicsAllocation *allocateGraphicsMemoryWithProperties(const AllocationProperties &properties) override {
            storedAllocationProperties.push_back(properties);
            return OsAgnosticMemoryManager::allocateGraphicsMemoryWithProperties(properties);
        }

        std::vector<AllocationProperties> storedAllocationProperties;
    };

    void SetUp() override {
        debugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

        auto executionEnvironment = new NEO::MockExecutionEnvironment(defaultHwInfo.get(), true, numRootDevices);
        executionEnvironment->rootDeviceEnvironments[0]->initGmm();

        memoryManager = new MyMemoryManager(*executionEnvironment);
        executionEnvironment->memoryManager.reset(memoryManager);

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0);
        std::vector<std::unique_ptr<NEO::Device>> devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->initialize(std::move(devices));

        device = driverHandle->devices[0];
    }

    VariableBackup<bool> mockDeviceFlagBackup{&NEO::MockDevice::createSingleDevice, false};
    DebugManagerStateRestore restore;

    NEO::MockDevice *neoDevice = nullptr;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    L0::Device *device = nullptr;
    MyMemoryManager *memoryManager = nullptr;
    const uint32_t numRootDevices = 1;
    const uint32_t numSubDevices = 4;
};

TEST_F(CommandQueueInitTests, givenMultipleSubDevicesWhenInitializingThenAllocateForAllSubDevices) {
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);

    memoryManager->storedAllocationProperties.clear();

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily, device, csr.get(), &desc, false, false, false, returnValue);
    EXPECT_NE(nullptr, commandQueue);

    const uint64_t expectedBitfield = maxNBitValue(numSubDevices);

    uint32_t cmdBufferAllocationsFound = 0;
    for (auto &allocationProperties : memoryManager->storedAllocationProperties) {
        if (allocationProperties.allocationType == NEO::AllocationType::commandBuffer) {
            cmdBufferAllocationsFound++;
            EXPECT_EQ(expectedBitfield, allocationProperties.subDevicesBitfield.to_ulong());
            EXPECT_EQ(1u, allocationProperties.flags.multiOsContextCapable);
        }
    }

    EXPECT_EQ(static_cast<uint32_t>(CommandQueueImp::CommandBufferManager::BufferAllocation::count), cmdBufferAllocationsFound);

    commandQueue->destroy();
}

TEST_F(CommandQueueInitTests, whenDestroyCommandQueueThenStoreCommandBuffersAsReusableAllocations) {
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily, device, csr.get(), &desc, false, false, false, returnValue);
    EXPECT_NE(nullptr, commandQueue);
    auto deviceImp = static_cast<DeviceImp *>(device);
    EXPECT_TRUE(deviceImp->allocationsForReuse->peekIsEmpty());

    commandQueue->destroy();

    EXPECT_FALSE(deviceImp->allocationsForReuse->peekIsEmpty());
}

struct DeviceWithDualStorage : Test<DeviceFixture> {
    void SetUp() override {

        debugManager.flags.EnableLocalMemory.set(1);
        debugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(1);
        DeviceFixture::setUp();

        mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    }
    void TearDown() override {
        DeviceFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
    std::unique_ptr<L0::ult::Module> mockModule;
};

HWTEST2_F(DeviceWithDualStorage, givenCmdListWithAppendedKernelAndUsmTransferAndBlitterDisabledWhenExecuteCmdListThenCfeStateOnceProgrammed, IsHeapfulRequiredAndAtLeastXeCore) {

    auto &compilerProductHelper = neoDevice->getCompilerProductHelper();
    if (compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    using CFE_STATE = typename FamilyType::CFE_STATE;
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<MockMemoryOperationsHandler>();
    ze_result_t res = ZE_RESULT_SUCCESS;

    const ze_command_queue_desc_t desc = {};
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getInternalEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          false,
                                                          res));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, commandQueue);

    auto commandList = std::unique_ptr<CommandList>(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, res, false)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, commandList);
    Mock<KernelImp> kernel;
    kernel.immutableData.device = device;
    kernel.module = mockModule.get();
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    res = context->allocSharedMem(device->toHandle(),
                                  &deviceDesc,
                                  &hostDesc,
                                  size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto gpuAlloc = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(ptr)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    kernel.privateState.argumentsResidencyContainer.push_back(gpuAlloc);

    ze_group_count_t dispatchKernelArguments{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel.toHandle(), dispatchKernelArguments, nullptr, 0, nullptr, launchParams);
    auto deviceImp = static_cast<DeviceImp *>(device);
    commandList->close();

    auto pageFaultCmdQueue = whiteboxCast(CommandList::whiteboxCast(deviceImp->pageFaultCommandList)->cmdQImmediate);

    auto pageFaultCsr = pageFaultCmdQueue->getCsr();
    auto &pageFaultCsrStream = pageFaultCsr->getCS(0);
    auto pageFaultCsrStreamBefore = pageFaultCsrStream.getUsed();

    auto sizeBefore = commandQueue->commandStream.getUsed();
    auto pageFaultSizeBefore = pageFaultCmdQueue->commandStream.getUsed();
    auto handle = commandList->toHandle();
    commandQueue->executeCommandLists(1, &handle, nullptr, true, nullptr, nullptr);
    auto sizeAfter = commandQueue->commandStream.getUsed();
    auto pageFaultSizeAfter = pageFaultCmdQueue->commandStream.getUsed();
    auto pageFaultCsrStreamAfter = pageFaultCsrStream.getUsed();
    EXPECT_LT(sizeBefore, sizeAfter);
    EXPECT_EQ(pageFaultSizeBefore, pageFaultSizeAfter);
    EXPECT_LT(pageFaultCsrStreamBefore, pageFaultCsrStreamAfter);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(commandQueue->commandStream.getCpuBase(), 0),
                                             sizeAfter);
    auto count = findAll<CFE_STATE *>(commands.begin(), commands.end()).size();
    EXPECT_EQ(0u, count);

    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(pageFaultCmdQueue->commandStream.getCpuBase(), 0),
                                             pageFaultSizeAfter);
    count = findAll<CFE_STATE *>(commands.begin(), commands.end()).size();
    EXPECT_EQ(0u, count);

    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(pageFaultCsrStream.getCpuBase(), 0),
                                             pageFaultCsrStreamAfter);
    count = findAll<CFE_STATE *>(commands.begin(), commands.end()).size();
    EXPECT_EQ(1u, count);

    res = context->freeMem(ptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
    commandQueue->destroy();
}
using CommandQueueScratchTests = Test<DeviceFixture>;

using Platforms = IsAtLeastProduct<IGFX_XE_HP_SDV>;

HWTEST2_F(CommandQueueScratchTests, givenCommandQueueWhenHandleScratchSpaceThenProperScratchSlotIsSetAndScratchAllocationReturned, Platforms) {
    class MockScratchSpaceControllerXeHPAndLater : public NEO::ScratchSpaceControllerXeHPAndLater {
      public:
        uint32_t scratchSlot = 0u;
        bool programHeapsCalled = false;
        NEO::GraphicsAllocation *scratchAllocation = nullptr;
        MockScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                               NEO::ExecutionEnvironment &environment,
                                               InternalAllocationStorage &allocationStorage) : NEO::ScratchSpaceControllerXeHPAndLater(rootDeviceIndex, environment, allocationStorage) {}

        void programHeaps(HeapContainer &heapContainer,
                          uint32_t scratchSlot,
                          uint32_t requiredPerThreadScratchSizeSlot0,
                          uint32_t requiredPerThreadScratchSizeSlot1,
                          OsContext &osContext,
                          bool &stateBaseAddressDirty,
                          bool &vfeStateDirty) override {
            this->scratchSlot = scratchSlot;
            programHeapsCalled = true;
        }

        NEO::GraphicsAllocation *getScratchSpaceSlot0Allocation() override {
            return scratchAllocation;
        }

      protected:
    };

    MockCommandStreamReceiver csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    NEO::ExecutionEnvironment *execEnv = neoDevice->getExecutionEnvironment();
    std::unique_ptr<ScratchSpaceController> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(device->getRootDeviceIndex(),
                                                                                                                         *execEnv,
                                                                                                                         *csr.getInternalAllocationStorage());

    const ze_command_queue_desc_t desc = {};

    std::unique_ptr<L0::CommandQueue> commandQueue = std::make_unique<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, &csr, &desc);
    auto commandQueueHw = static_cast<MockCommandQueueHw<FamilyType::gfxCoreFamily> *>(commandQueue.get());

    NEO::ResidencyContainer residencyContainer;
    NEO::HeapContainer heapContainer;

    void *surfaceHeap = alignedMalloc(0x1000, 0x1000);
    NEO::GraphicsAllocation graphicsAllocationHeap(0, 1u /*num gmms*/, NEO::AllocationType::buffer, surfaceHeap, 0u, 0u, 1u, MemoryPool::system4KBPages, 1u);
    heapContainer.push_back(&graphicsAllocationHeap);
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;

    NEO::GraphicsAllocation graphicsAllocation(1u, 1u /*num gmms*/, NEO::AllocationType::buffer, nullptr, 0u, 0u, 0u, MemoryPool::system4KBPages, 0u);

    auto scratch = static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get());
    scratch->scratchAllocation = &graphicsAllocation;
    commandQueueHw->handleScratchSpace(heapContainer, scratchController.get(), nullptr, gsbaStateDirty, frontEndStateDirty, 0x1000, 0u);

    EXPECT_TRUE(scratch->programHeapsCalled);
    EXPECT_GT(csr.makeResidentCalledTimes, 0u);

    alignedFree(surfaceHeap);
}

HWTEST2_F(CommandQueueScratchTests, givenCommandQueueWhenHandleScratchSpaceAndHeapContainerIsZeroSizeThenNoFunctionIsCalled, Platforms) {
    class MockScratchSpaceControllerXeHPAndLater : public NEO::ScratchSpaceControllerXeHPAndLater {
      public:
        using NEO::ScratchSpaceControllerXeHPAndLater::scratchSlot0Allocation;
        bool programHeapsCalled = false;
        MockScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                               NEO::ExecutionEnvironment &environment,
                                               InternalAllocationStorage &allocationStorage) : NEO::ScratchSpaceControllerXeHPAndLater(rootDeviceIndex, environment, allocationStorage) {}

        void programHeaps(HeapContainer &heapContainer,
                          uint32_t scratchSlot,
                          uint32_t requiredPerThreadScratchSizeSlot0,
                          uint32_t requiredPerThreadScratchSizeSlot1,
                          OsContext &osContext,
                          bool &stateBaseAddressDirty,
                          bool &vfeStateDirty) override {
            programHeapsCalled = true;
        }

      protected:
    };

    MockCommandStreamReceiver csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    NEO::ExecutionEnvironment *execEnv = neoDevice->getExecutionEnvironment();
    std::unique_ptr<ScratchSpaceController> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(device->getRootDeviceIndex(),
                                                                                                                         *execEnv,
                                                                                                                         *csr.getInternalAllocationStorage());

    const ze_command_queue_desc_t desc = {};

    std::unique_ptr<L0::CommandQueue> commandQueue = std::make_unique<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, &csr, &desc);
    auto commandQueueHw = static_cast<MockCommandQueueHw<FamilyType::gfxCoreFamily> *>(commandQueue.get());

    NEO::ResidencyContainer residencyContainer;
    NEO::HeapContainer heapContainer;
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;

    NEO::GraphicsAllocation graphicsAllocation(1u, 1u /*num gmms*/, NEO::AllocationType::buffer, nullptr, 0u, 0u, 0u, MemoryPool::system4KBPages, 0u);

    auto scratch = static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get());
    scratch->scratchSlot0Allocation = &graphicsAllocation;
    commandQueueHw->handleScratchSpace(heapContainer, scratchController.get(), nullptr, gsbaStateDirty, frontEndStateDirty, 0x1000, 0u);

    EXPECT_FALSE(scratch->programHeapsCalled);
    scratch->scratchSlot0Allocation = nullptr;
}

HWTEST2_F(CommandQueueScratchTests, whenPatchCommandsIsCalledThenCommandsAreCorrectlyPatched, IsAtLeastXeCore) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = std::make_unique<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr, &desc);
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();

    EXPECT_NO_THROW(commandQueue->patchCommands(*commandList, 0, false, nullptr));
    commandList->commandsToPatch.push_back({});
    EXPECT_ANY_THROW(commandQueue->patchCommands(*commandList, 0, false, nullptr));
    commandList->commandsToPatch.clear();

    if constexpr (FamilyType::isHeaplessRequired()) {
        CommandToPatch commandToPatch;
        commandToPatch.pDestination = nullptr;
        commandToPatch.pCommand = nullptr;
        commandToPatch.type = CommandToPatch::FrontEndState;
        commandList->commandsToPatch.push_back(commandToPatch);
        EXPECT_ANY_THROW(commandQueue->patchCommands(*commandList, 0, false, nullptr));
        commandList->commandsToPatch.clear();
    } else {
        using CFE_STATE = typename FamilyType::CFE_STATE;
        CFE_STATE destinationCfeStates[4];
        int32_t initialScratchAddress = 0x123400;
        for (size_t i = 0; i < 4; i++) {
            auto sourceCfeState = new CFE_STATE;
            *sourceCfeState = FamilyType::cmdInitCfeState;
            if constexpr (TestTraits<FamilyType::gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
                sourceCfeState->setNumberOfWalkers(2);
            }
            sourceCfeState->setMaximumNumberOfThreads(16);
            sourceCfeState->setScratchSpaceBuffer(initialScratchAddress);

            destinationCfeStates[i] = FamilyType::cmdInitCfeState;
            if constexpr (TestTraits<FamilyType::gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
                EXPECT_NE(destinationCfeStates[i].getNumberOfWalkers(), sourceCfeState->getNumberOfWalkers());
            }
            EXPECT_NE(destinationCfeStates[i].getMaximumNumberOfThreads(), sourceCfeState->getMaximumNumberOfThreads());

            CommandToPatch commandToPatch;
            commandToPatch.pDestination = &destinationCfeStates[i];
            commandToPatch.pCommand = sourceCfeState;
            commandToPatch.type = CommandToPatch::CommandType::FrontEndState;
            commandList->commandsToPatch.push_back(commandToPatch);
        }

        uint64_t patchedScratchAddress = 0xABCD00;
        commandQueue->patchCommands(*commandList, patchedScratchAddress, false, nullptr);
        for (size_t i = 0; i < 4; i++) {
            EXPECT_EQ(patchedScratchAddress, destinationCfeStates[i].getScratchSpaceBuffer());
            auto &sourceCfeState = *reinterpret_cast<CFE_STATE *>(commandList->commandsToPatch[i].pCommand);
            if constexpr (TestTraits<FamilyType::gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
                EXPECT_EQ(destinationCfeStates[i].getNumberOfWalkers(), sourceCfeState.getNumberOfWalkers());
            }
            EXPECT_EQ(destinationCfeStates[i].getMaximumNumberOfThreads(), sourceCfeState.getMaximumNumberOfThreads());
            EXPECT_EQ(destinationCfeStates[i].getScratchSpaceBuffer(), sourceCfeState.getScratchSpaceBuffer());
        }
    }
}

HWTEST2_F(CommandQueueScratchTests, givenCommandsToPatchToNotSupportedPlatformWhenPatchCommandsIsCalledThenAbortIsThrown, IsGen12LP) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = std::make_unique<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr, &desc);
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();

    EXPECT_NO_THROW(commandQueue->patchCommands(*commandList, 0, false, nullptr));
    commandList->commandsToPatch.push_back({});
    EXPECT_ANY_THROW(commandQueue->patchCommands(*commandList, 0, false, nullptr));
    commandList->commandsToPatch.clear();

    CommandToPatch commandToPatch;

    commandToPatch.type = CommandToPatch::FrontEndState;
    commandList->commandsToPatch.push_back(commandToPatch);
    EXPECT_ANY_THROW(commandQueue->patchCommands(*commandList, 0, false, nullptr));
    commandList->commandsToPatch.clear();

    commandToPatch.type = CommandToPatch::Invalid;
    commandList->commandsToPatch.push_back(commandToPatch);
    EXPECT_ANY_THROW(commandQueue->patchCommands(*commandList, 0, false, nullptr));
    commandList->commandsToPatch.clear();
}

HWTEST2_F(CommandQueueScratchTests, givenInlineDataScratchWhenPatchCommandsIsCalledThenCommandsAreCorrectlyPatched, IsAtLeastXeCore) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = std::make_unique<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr, &desc);
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();

    constexpr uint64_t dummyScratchAddress = 0xdeadu;
    constexpr uint64_t baseAddress = 0x1234u;
    constexpr uint64_t scratchAddress = 0x5000u;
    constexpr uint64_t expectedPatchedScratchAddress = baseAddress + scratchAddress;

    struct TestCase {
        bool undefinedPatchSize;
        bool undefinedOffset;
        bool scratchAlreadyPatched;
        bool scratchControllerChanged;
        uint64_t expectedValue;
    };

    const TestCase testCases[] = {
        {false, false, false, false, expectedPatchedScratchAddress}, // Valid patchSize and offset - should patch
        {false, false, false, true, expectedPatchedScratchAddress},  // scratchControllerChanged - should patch
        {true, false, false, false, dummyScratchAddress},            // Undefined patchSize - should not patch
        {false, true, false, false, dummyScratchAddress},            // Undefined offset - should not patch
        {false, false, true, false, dummyScratchAddress},            // scratchAddressAfterPatch==scratchAddress - should not patch
    };

    for (const auto &testCase : testCases) {
        uint64_t scratchBuffer = dummyScratchAddress;
        commandList->commandsToPatch.clear();

        CommandToPatch cmd;
        cmd.type = CommandToPatch::ComputeWalkerInlineDataScratch;
        cmd.pDestination = &scratchBuffer;
        cmd.offset = testCase.undefinedOffset ? NEO::undefined<size_t> : 0;
        cmd.patchSize = testCase.undefinedPatchSize ? NEO::undefined<size_t> : sizeof(uint64_t);
        cmd.baseAddress = baseAddress;
        cmd.scratchAddressAfterPatch = testCase.scratchAlreadyPatched ? scratchAddress : 0;

        commandList->commandsToPatch.push_back(cmd);
        commandQueue->patchCommands(*commandList, scratchAddress, testCase.scratchControllerChanged, nullptr);

        EXPECT_EQ(testCase.expectedValue, scratchBuffer);
    }
}

HWTEST2_F(CommandQueueScratchTests, givenImplicitArgsScratchWhenPatchCommandsIsCalledThenCommandsAreCorrectlyPatched, IsAtLeastXeCore) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = std::make_unique<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr, &desc);
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();

    constexpr uint64_t dummyScratchAddress = 0xdeadu;
    constexpr uint64_t baseAddress = 0x1234u;
    constexpr uint64_t scratchAddress = 0x5000u;
    constexpr uint64_t expectedPatchedScratchAddress = baseAddress + scratchAddress;

    for (const bool scratchControllerChanged : {false, true}) {
        for (const bool scratchAlreadyPatched : {false, true}) {
            const uint64_t expectedValue = (scratchControllerChanged || !scratchAlreadyPatched) ? expectedPatchedScratchAddress : dummyScratchAddress;
            uint64_t scratchBuffer = dummyScratchAddress;
            commandList->commandsToPatch.clear();

            CommandToPatch cmd;
            cmd.type = CommandToPatch::ComputeWalkerImplicitArgsScratch;
            cmd.pDestination = &scratchBuffer;
            cmd.baseAddress = baseAddress;
            cmd.offset = 0;
            cmd.patchSize = sizeof(uint64_t);
            cmd.scratchAddressAfterPatch = scratchAlreadyPatched ? scratchAddress : 0;

            commandList->commandsToPatch.push_back(cmd);
            commandQueue->patchCommands(*commandList, scratchAddress, scratchControllerChanged, nullptr);

            EXPECT_EQ(expectedValue, scratchBuffer);
        }
    }
}

using CommandQueueCreate = Test<DeviceFixture>;

HWTEST_F(CommandQueueCreate, givenCommandsToPatchWithNoopSpacePatchWhenPatchCommandsIsCalledThenSpaceIsNooped) {
    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = std::make_unique<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr, &desc);
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();

    constexpr uint32_t dataSize = 64;
    auto patchBuffer = std::make_unique<uint8_t[]>(dataSize);
    auto zeroBuffer = std::make_unique<uint8_t[]>(dataSize);
    memset(patchBuffer.get(), 0xFF, dataSize);
    memset(zeroBuffer.get(), 0x0, dataSize);

    CommandToPatch commandToPatch;

    commandToPatch.type = CommandToPatch::NoopSpace;
    commandToPatch.pDestination = patchBuffer.get();
    commandToPatch.patchSize = dataSize;

    commandList->commandsToPatch.push_back(commandToPatch);
    commandQueue->patchCommands(*commandList, 0, false, nullptr);
    EXPECT_EQ(0, memcmp(patchBuffer.get(), zeroBuffer.get(), dataSize));

    memset(patchBuffer.get(), 0xFF, dataSize);
    commandList->commandsToPatch[0].pDestination = nullptr;
    commandQueue->patchCommands(*commandList, 0, false, nullptr);
    EXPECT_NE(0, memcmp(patchBuffer.get(), zeroBuffer.get(), dataSize));
}

using HostFunctionsCmdPatchTests = Test<DeviceFixture>;

HWTEST_F(HostFunctionsCmdPatchTests, givenHostFunctionPatchCommandsWhenPatchCommandsIsCalledThenCorrectInstructionsWereProgrammed) {

    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = std::make_unique<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr, &desc);
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->commandsToPatch.clear();

    constexpr uint64_t pHostFunction = std::numeric_limits<uint64_t>::max() - 1024u;
    constexpr uint64_t pUserData = std::numeric_limits<uint64_t>::max() - 4096u;

    MI_STORE_DATA_IMM callbackAddressMiStore{};
    MI_STORE_DATA_IMM userDataMiStore{};
    MI_STORE_DATA_IMM internalTagMiStore{};
    MI_SEMAPHORE_WAIT internalTagMiWait{};

    {
        CommandToPatch commandToPatch;
        commandToPatch.type = CommandToPatch::HostFunctionEntry;
        commandToPatch.baseAddress = pHostFunction;
        commandToPatch.pCommand = reinterpret_cast<void *>(&callbackAddressMiStore);
        commandList->commandsToPatch.push_back(commandToPatch);
    }
    {
        CommandToPatch commandToPatch;
        commandToPatch.type = CommandToPatch::HostFunctionUserData;
        commandToPatch.baseAddress = pUserData;
        commandToPatch.pCommand = reinterpret_cast<void *>(&userDataMiStore);
        commandList->commandsToPatch.push_back(commandToPatch);
    }
    {
        CommandToPatch commandToPatch;
        commandToPatch.type = CommandToPatch::HostFunctionSignalInternalTag;
        commandToPatch.pCommand = reinterpret_cast<void *>(&internalTagMiStore);
        commandList->commandsToPatch.push_back(commandToPatch);
    }
    {
        CommandToPatch commandToPatch;
        commandToPatch.type = CommandToPatch::HostFunctionWaitInternalTag;
        commandToPatch.pCommand = reinterpret_cast<void *>(&internalTagMiWait);
        commandList->commandsToPatch.push_back(commandToPatch);
    }

    commandQueue->patchCommands(*commandList, 0, false, nullptr);

    EXPECT_NE(nullptr, commandQueue->csr->getHostFunctionDataAllocation());

    auto &hostFunctionDataFromCsr = commandQueue->csr->getHostFunctionData();

    // callback address - mi store
    EXPECT_EQ(getLowPart(pHostFunction), callbackAddressMiStore.getDataDword0());
    EXPECT_EQ(getHighPart(pHostFunction), callbackAddressMiStore.getDataDword1());
    EXPECT_TRUE(callbackAddressMiStore.getStoreQword());
    EXPECT_EQ(reinterpret_cast<uint64_t>(hostFunctionDataFromCsr.entry), callbackAddressMiStore.getAddress());

    // userData address - mi store
    EXPECT_EQ(getLowPart(pUserData), userDataMiStore.getDataDword0());
    EXPECT_EQ(getHighPart(pUserData), userDataMiStore.getDataDword1());
    EXPECT_TRUE(userDataMiStore.getStoreQword());
    EXPECT_EQ(reinterpret_cast<uint64_t>(hostFunctionDataFromCsr.userData), userDataMiStore.getAddress());

    // internal tag signal - mi store
    EXPECT_EQ(static_cast<uint32_t>(HostFunctionTagStatus::pending), internalTagMiStore.getDataDword0());
    EXPECT_FALSE(internalTagMiStore.getStoreQword());
    EXPECT_EQ(reinterpret_cast<uint64_t>(hostFunctionDataFromCsr.internalTag), internalTagMiStore.getAddress());

    // internal tag wait - semaphore wait
    EXPECT_EQ(static_cast<uint32_t>(HostFunctionTagStatus::completed), internalTagMiWait.getSemaphoreDataDword());
    EXPECT_EQ(reinterpret_cast<uint64_t>(hostFunctionDataFromCsr.internalTag), internalTagMiWait.getSemaphoreGraphicsAddress());
}

} // namespace ult
} // namespace L0
