/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/scratch_space_controller_xehp_and_later.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
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

#include "test_traits_common.h"

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
    subDevice0.getRegularEngineGroups().back().engineGroupType = EngineGroupType::Compute;
    subDevice0.getRegularEngineGroups().back().engines.resize(1);
    subDevice0.getRegularEngineGroups().back().engines[0].commandStreamReceiver = &rootDevice.getGpgpuCommandStreamReceiver();
    auto ordinal = static_cast<uint32_t>(subDevice0.getRegularEngineGroups().size() - 1);
    Mock<L0::DeviceImp> l0RootDevice(&rootDevice, rootDevice.getExecutionEnvironment());

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

HWTEST_TEMPLATED_F(AubCsrTest, givenAubCsrWhenCallingExecuteCommandListsThenPollForCompletionIsCalled) {
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    ze_result_t returnValue;
    ze_command_queue_desc_t desc = {};
    ze_command_queue_handle_t commandQueue = {};
    ze_result_t res = context->createCommandQueue(device, &desc, &commandQueue);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, commandQueue);

    auto aubCsr = static_cast<NEO::UltAubCommandStreamReceiver<FamilyType> *>(csr);
    CommandQueue *queue = static_cast<CommandQueue *>(L0::CommandQueue::fromHandle(commandQueue));
    EXPECT_EQ(aubCsr->pollForCompletionCalled, 0u);

    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto commandListHandle = commandList->toHandle();
    queue->executeCommandLists(1, &commandListHandle, nullptr, false);
    EXPECT_EQ(aubCsr->pollForCompletionCalled, 1u);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

using CommandQueueSynchronizeTest = Test<DeviceFixture>;
using MultiTileCommandQueueSynchronizeTest = Test<SingleRootMultiSubDeviceFixture>;

template <typename GfxFamily>
struct SynchronizeCsr : public NEO::UltCommandStreamReceiver<GfxFamily> {
    SynchronizeCsr(const NEO::ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
        : NEO::UltCommandStreamReceiver<GfxFamily>(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment), 0, deviceBitfield) {
        CommandStreamReceiver::tagAddress = &tagAddressData[0];
        memset(const_cast<uint32_t *>(CommandStreamReceiver::tagAddress), 0xFFFFFFFF, tagSize * sizeof(uint32_t));
    }

    WaitStatus waitForCompletionWithTimeout(const WaitParams &params, uint32_t taskCountToWait) override {
        enableTimeoutSet = params.enableTimeout;
        waitForComplitionCalledTimes++;
        partitionCountSet = this->activePartitions;
        return waitForCompletionWithTimeoutResult;
    }

    WaitStatus waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool quickKmdSleep, NEO::QueueThrottle throttle) override {
        waitForTaskCountWithKmdNotifyFallbackCalled++;
        return NEO::UltCommandStreamReceiver<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(taskCountToWait, flushStampToWait, quickKmdSleep, throttle);
    }

    static constexpr size_t tagSize = 128;
    static volatile uint32_t tagAddressData[tagSize];
    uint32_t waitForComplitionCalledTimes = 0;
    uint32_t waitForTaskCountWithKmdNotifyFallbackCalled = 0;
    uint32_t partitionCountSet = 0;
    bool enableTimeoutSet = false;
    WaitStatus waitForCompletionWithTimeoutResult = WaitStatus::Ready;
};

template <typename GfxFamily>
volatile uint32_t SynchronizeCsr<GfxFamily>::tagAddressData[SynchronizeCsr<GfxFamily>::tagSize];

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
    csr->waitForCompletionWithTimeoutResult = NEO::WaitStatus::GpuHang;

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
    NEO::DebugManager.flags.OverrideUseKmdWaitFunction.set(1);

    auto csr = std::unique_ptr<SynchronizeCsr<FamilyType>>(new SynchronizeCsr<FamilyType>(*device->getNEODevice()->getExecutionEnvironment(),
                                                                                          device->getNEODevice()->getDeviceBitfield()));
    csr->waitForCompletionWithTimeoutResult = NEO::WaitStatus::GpuHang;

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
    NEO::DebugManager.flags.OverrideUseKmdWaitFunction.set(1);

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

HWTEST2_F(MultiTileCommandQueueSynchronizeTest, givenMultiplePartitionCountWhenCallingSynchronizeThenExpectTheSameNumberCsrSynchronizeCalls, IsAtLeastXeHpCore) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto csr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);
    if (device->getNEODevice()->getPreemptionMode() == PreemptionMode::MidThread || device->getNEODevice()->isDebuggerActive()) {
        csr->createPreemptionAllocation();
    }
    EXPECT_NE(0u, csr->getPostSyncWriteOffset());
    volatile uint32_t *tagAddress = csr->getTagAddress();
    for (uint32_t i = 0; i < 2; i++) {
        *tagAddress = 0xFF;
        tagAddress = ptrOffset(tagAddress, csr->getPostSyncWriteOffset());
    }
    csr->activePartitions = 2u;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(2u, commandQueue->activeSubDevices);

    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);
    commandList->partitionCount = 2;

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    uint64_t timeout = std::numeric_limits<uint64_t>::max();
    commandQueue->synchronize(timeout);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST2_F(MultiTileCommandQueueSynchronizeTest, givenCsrHasMultipleActivePartitionWhenExecutingCmdListOnNewCmdQueueThenExpectCmdPartitionCountMatchCsrActivePartitions, IsAtLeastXeHpCore) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto csr = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(neoDevice->getDefaultEngine().commandStreamReceiver);
    if (device->getNEODevice()->getPreemptionMode() == PreemptionMode::MidThread || device->getNEODevice()->isDebuggerActive()) {
        csr->createPreemptionAllocation();
    }
    EXPECT_NE(0u, csr->getPostSyncWriteOffset());
    volatile uint32_t *tagAddress = csr->getTagAddress();
    for (uint32_t i = 0; i < 2; i++) {
        *tagAddress = 0xFF;
        tagAddress = ptrOffset(tagAddress, csr->getPostSyncWriteOffset());
    }
    csr->activePartitions = 2u;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(2u, commandQueue->activeSubDevices);

    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    EXPECT_EQ(2u, commandQueue->partitionCount);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

HWTEST_F(CommandQueueSynchronizeTest, givenSingleTileCsrWhenExecutingMultiTileCommandListThenExpectErrorOnExecute) {
    const ze_command_queue_desc_t desc{};
    ze_result_t returnValue;

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(1u, commandQueue->activeSubDevices);

    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);
    commandList->partitionCount = 2;

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE);

    L0::CommandQueue::fromHandle(commandQueue)->destroy();
}

template <typename GfxFamily>
struct TestCmdQueueCsr : public NEO::UltCommandStreamReceiver<GfxFamily> {
    TestCmdQueueCsr(const NEO::ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
        : NEO::UltCommandStreamReceiver<GfxFamily>(const_cast<NEO::ExecutionEnvironment &>(executionEnvironment), 0, deviceBitfield) {
    }

    ADDMETHOD_NOBASE(waitForCompletionWithTimeout, NEO::WaitStatus, NEO::WaitStatus::NotReady, (const WaitParams &params, uint32_t taskCountToWait));
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
    using PARSE = typename FamilyType::PARSE;

    DebugManagerStateRestore restore;
    NEO::DebugManager.flags.UpdateTaskCountFromWait.set(3);

    size_t expectedSize = sizeof(MI_BATCH_BUFFER_START);
    if (neoDevice->getDefaultEngine().commandStreamReceiver->isAnyDirectSubmissionEnabled()) {
        expectedSize += sizeof(MI_BATCH_BUFFER_START);
    } else {
        expectedSize += sizeof(MI_BATCH_BUFFER_END);
    }
    expectedSize += NEO::MemorySynchronizationCommands<FamilyType>::getSizeForBarrierWithPostSyncOperation(neoDevice->getHardwareInfo());
    expectedSize = alignUp(expectedSize, 8);

    const ze_command_queue_desc_t desc{ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, nullptr, 0, 0, 0, ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, ZE_COMMAND_QUEUE_PRIORITY_NORMAL};
    ze_result_t returnValue;

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                          device,
                                                          neoDevice->getDefaultEngine().commandStreamReceiver,
                                                          &desc,
                                                          false,
                                                          false,
                                                          returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandQueue);
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, commandQueue->getSynchronousMode());

    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));
    ASSERT_NE(nullptr, commandList);

    //1st execute provides all preamble commands
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    size_t executionConsumedSize = usedSpaceAfter - usedSpaceBefore;

    EXPECT_EQ(expectedSize, executionConsumedSize);

    GenCmdList cmdList;
    ASSERT_TRUE(PARSE::parseCommandBuffer(cmdList,
                                          ptrOffset(commandQueue->commandStream->getCpuBase(), usedSpaceBefore),
                                          executionConsumedSize));

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    size_t pipeControlsPostSyncNumber = 0u;
    uint32_t expectedData = commandQueue->getCsr()->peekTaskCount();
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
            executionEnvironment->rootDeviceEnvironments[i]->setHwInfo(NEO::defaultHwInfo.get());
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
        DebugManager.flags.CreateMultipleSubDevices.set(numSubDevices);

        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(numRootDevices);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(NEO::defaultHwInfo.get());
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

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily, device, csr.get(), &desc, false, false, returnValue);
    EXPECT_NE(nullptr, commandQueue);

    const uint64_t expectedBitfield = maxNBitValue(numSubDevices);

    uint32_t cmdBufferAllocationsFound = 0;
    for (auto &allocationProperties : memoryManager->storedAllocationProperties) {
        if (allocationProperties.allocationType == NEO::AllocationType::COMMAND_BUFFER) {
            cmdBufferAllocationsFound++;
            EXPECT_EQ(expectedBitfield, allocationProperties.subDevicesBitfield.to_ulong());
            EXPECT_EQ(1u, allocationProperties.flags.multiOsContextCapable);
        }
    }

    EXPECT_EQ(static_cast<uint32_t>(CommandQueueImp::CommandBufferManager::BUFFER_ALLOCATION::COUNT), cmdBufferAllocationsFound);

    commandQueue->destroy();
}

TEST_F(CommandQueueInitTests, whenDestroyCommandQueueThenStoreCommandBuffersAsReusableAllocations) {
    ze_command_queue_desc_t desc = {};
    auto csr = std::unique_ptr<NEO::CommandStreamReceiver>(neoDevice->createCommandStreamReceiver());
    csr->setupContext(*neoDevice->getDefaultEngine().osContext);

    ze_result_t returnValue;
    L0::CommandQueue *commandQueue = CommandQueue::create(productFamily, device, csr.get(), &desc, false, false, returnValue);
    EXPECT_NE(nullptr, commandQueue);
    auto deviceImp = static_cast<DeviceImp *>(device);
    EXPECT_TRUE(deviceImp->allocationsForReuse->peekIsEmpty());

    commandQueue->destroy();

    EXPECT_FALSE(deviceImp->allocationsForReuse->peekIsEmpty());
}

struct DeviceWithDualStorage : Test<DeviceFixture> {
    void SetUp() override {
        NEO::MockCompilerEnableGuard mock(true);
        DebugManager.flags.EnableLocalMemory.set(1);
        DebugManager.flags.AllocateSharedAllocationsWithCpuAndGpuStorage.set(1);
        DeviceFixture::setUp();
    }
    void TearDown() override {
        DeviceFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
};

HWTEST2_F(DeviceWithDualStorage, givenCmdListWithAppendedKernelAndUsmTransferAndBlitterDisabledWhenExecuteCmdListThenCfeStateOnceProgrammed, IsAtLeastXeHpCore) {
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
                                                          res));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, commandQueue);

    auto commandList = std::unique_ptr<CommandList>(whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, res)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    ASSERT_NE(nullptr, commandList);
    Mock<Kernel> kernel;
    kernel.immutableData.device = device;
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
    kernel.residencyContainer.push_back(gpuAlloc);

    ze_group_count_t dispatchFunctionArguments{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    commandList->appendLaunchKernel(kernel.toHandle(), &dispatchFunctionArguments, nullptr, 0, nullptr, launchParams);
    auto deviceImp = static_cast<DeviceImp *>(device);
    auto pageFaultCmdQueue = whiteboxCast(deviceImp->pageFaultCommandList->cmdQImmediate);

    auto sizeBefore = commandQueue->commandStream->getUsed();
    auto pageFaultSizeBefore = pageFaultCmdQueue->commandStream->getUsed();
    auto handle = commandList->toHandle();
    commandQueue->executeCommandLists(1, &handle, nullptr, true);
    auto sizeAfter = commandQueue->commandStream->getUsed();
    auto pageFaultSizeAfter = pageFaultCmdQueue->commandStream->getUsed();
    EXPECT_LT(sizeBefore, sizeAfter);
    EXPECT_LT(pageFaultSizeBefore, pageFaultSizeAfter);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(commandQueue->commandStream->getCpuBase(), 0),
                                             sizeAfter);
    auto count = findAll<CFE_STATE *>(commands.begin(), commands.end()).size();
    EXPECT_EQ(0u, count);

    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(pageFaultCmdQueue->commandStream->getCpuBase(), 0),
                                             pageFaultSizeAfter);
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
                          uint32_t requiredPerThreadScratchSize,
                          uint32_t requiredPerThreadPrivateScratchSize,
                          uint32_t currentTaskCount,
                          OsContext &osContext,
                          bool &stateBaseAddressDirty,
                          bool &vfeStateDirty) override {
            this->scratchSlot = scratchSlot;
            programHeapsCalled = true;
        }

        NEO::GraphicsAllocation *getScratchSpaceAllocation() override {
            return scratchAllocation;
        }

      protected:
    };

    MockCommandStreamReceiver csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    NEO::ExecutionEnvironment *execEnv = static_cast<NEO::ExecutionEnvironment *>(device->getExecEnvironment());
    std::unique_ptr<ScratchSpaceController> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(device->getRootDeviceIndex(),
                                                                                                                         *execEnv,
                                                                                                                         *csr.getInternalAllocationStorage());

    const ze_command_queue_desc_t desc = {};

    std::unique_ptr<L0::CommandQueue> commandQueue = std::make_unique<MockCommandQueueHw<gfxCoreFamily>>(device, &csr, &desc);
    auto commandQueueHw = static_cast<MockCommandQueueHw<gfxCoreFamily> *>(commandQueue.get());

    NEO::ResidencyContainer residencyContainer;
    NEO::HeapContainer heapContainer;

    void *surfaceHeap = alignedMalloc(0x1000, 0x1000);
    NEO::GraphicsAllocation graphicsAllocationHeap(0, NEO::AllocationType::BUFFER, surfaceHeap, 0u, 0u, 1u, MemoryPool::System4KBPages, 1u);
    heapContainer.push_back(&graphicsAllocationHeap);
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;

    NEO::GraphicsAllocation graphicsAllocation(1u, NEO::AllocationType::BUFFER, nullptr, 0u, 0u, 0u, MemoryPool::System4KBPages, 0u);

    auto scratch = static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get());
    scratch->scratchAllocation = &graphicsAllocation;
    commandQueueHw->handleScratchSpace(heapContainer, scratchController.get(), gsbaStateDirty, frontEndStateDirty, 0x1000, 0u);

    EXPECT_TRUE(scratch->programHeapsCalled);
    EXPECT_GT(csr.makeResidentCalledTimes, 0u);

    alignedFree(surfaceHeap);
}

HWTEST2_F(CommandQueueScratchTests, givenCommandQueueWhenHandleScratchSpaceAndHeapContainerIsZeroSizeThenNoFunctionIsCalled, Platforms) {
    class MockScratchSpaceControllerXeHPAndLater : public NEO::ScratchSpaceControllerXeHPAndLater {
      public:
        using NEO::ScratchSpaceControllerXeHPAndLater::scratchAllocation;
        bool programHeapsCalled = false;
        MockScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                               NEO::ExecutionEnvironment &environment,
                                               InternalAllocationStorage &allocationStorage) : NEO::ScratchSpaceControllerXeHPAndLater(rootDeviceIndex, environment, allocationStorage) {}

        void programHeaps(HeapContainer &heapContainer,
                          uint32_t scratchSlot,
                          uint32_t requiredPerThreadScratchSize,
                          uint32_t requiredPerThreadPrivateScratchSize,
                          uint32_t currentTaskCount,
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

    NEO::ExecutionEnvironment *execEnv = static_cast<NEO::ExecutionEnvironment *>(device->getExecEnvironment());
    std::unique_ptr<ScratchSpaceController> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(device->getRootDeviceIndex(),
                                                                                                                         *execEnv,
                                                                                                                         *csr.getInternalAllocationStorage());

    const ze_command_queue_desc_t desc = {};

    std::unique_ptr<L0::CommandQueue> commandQueue = std::make_unique<MockCommandQueueHw<gfxCoreFamily>>(device, &csr, &desc);
    auto commandQueueHw = static_cast<MockCommandQueueHw<gfxCoreFamily> *>(commandQueue.get());

    NEO::ResidencyContainer residencyContainer;
    NEO::HeapContainer heapContainer;
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;

    NEO::GraphicsAllocation graphicsAllocation(1u, NEO::AllocationType::BUFFER, nullptr, 0u, 0u, 0u, MemoryPool::System4KBPages, 0u);

    auto scratch = static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get());
    scratch->scratchAllocation = &graphicsAllocation;
    commandQueueHw->handleScratchSpace(heapContainer, scratchController.get(), gsbaStateDirty, frontEndStateDirty, 0x1000, 0u);

    EXPECT_FALSE(scratch->programHeapsCalled);
    scratch->scratchAllocation = nullptr;
}

HWTEST2_F(CommandQueueScratchTests, givenCommandQueueWhenBindlessEnabledThenHandleScratchSpaceCallsProgramBindlessSurfaceStateForScratch, Platforms) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.UseBindlessMode.set(1);
    class MockScratchSpaceControllerXeHPAndLater : public NEO::ScratchSpaceControllerXeHPAndLater {
      public:
        bool programHeapsCalled = false;
        NEO::MockGraphicsAllocation alloc;
        MockScratchSpaceControllerXeHPAndLater(uint32_t rootDeviceIndex,
                                               NEO::ExecutionEnvironment &environment,
                                               InternalAllocationStorage &allocationStorage) : NEO::ScratchSpaceControllerXeHPAndLater(rootDeviceIndex, environment, allocationStorage) {}

        void programBindlessSurfaceStateForScratch(BindlessHeapsHelper *heapsHelper,
                                                   uint32_t requiredPerThreadScratchSize,
                                                   uint32_t requiredPerThreadPrivateScratchSize,
                                                   uint32_t currentTaskCount,
                                                   OsContext &osContext,
                                                   bool &stateBaseAddressDirty,
                                                   bool &vfeStateDirty,
                                                   NEO::CommandStreamReceiver *csr) override {
            programHeapsCalled = true;
        }

        NEO::GraphicsAllocation *getScratchSpaceAllocation() override {
            return &alloc;
        }

      protected:
    };
    MockCsrHw2<FamilyType> csr(*neoDevice->getExecutionEnvironment(), 0, neoDevice->getDeviceBitfield());
    csr.initializeTagAllocation();
    csr.setupContext(*neoDevice->getDefaultEngine().osContext);

    NEO::ExecutionEnvironment *execEnv = static_cast<NEO::ExecutionEnvironment *>(device->getExecEnvironment());
    std::unique_ptr<ScratchSpaceController> scratchController = std::make_unique<MockScratchSpaceControllerXeHPAndLater>(device->getRootDeviceIndex(),
                                                                                                                         *execEnv,
                                                                                                                         *csr.getInternalAllocationStorage());
    const ze_command_queue_desc_t desc = {};

    std::unique_ptr<L0::CommandQueue> commandQueue = std::make_unique<MockCommandQueueHw<gfxCoreFamily>>(device, &csr, &desc);
    auto commandQueueHw = static_cast<MockCommandQueueHw<gfxCoreFamily> *>(commandQueue.get());

    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    NEO::ResidencyContainer residency;
    NEO::HeapContainer heapContainer;

    // scratch part
    commandQueueHw->handleScratchSpace(heapContainer, scratchController.get(), gsbaStateDirty, frontEndStateDirty, 0x1000, 0u);

    EXPECT_TRUE(static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get())->programHeapsCalled);

    // private part
    static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get())->programHeapsCalled = false;

    commandQueueHw->handleScratchSpace(heapContainer, scratchController.get(), gsbaStateDirty, frontEndStateDirty, 0x0, 0x1000);

    EXPECT_TRUE(static_cast<MockScratchSpaceControllerXeHPAndLater *>(scratchController.get())->programHeapsCalled);
}

HWTEST2_F(CommandQueueScratchTests, whenPatchCommandsIsCalledThenCommandsAreCorrectlyPatched, IsAtLeastXeHpCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u);
    auto commandQueue = std::make_unique<MockCommandQueueHw<gfxCoreFamily>>(device, csr, &desc);
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();

    EXPECT_NO_THROW(commandQueue->patchCommands(*commandList, 0));
    commandList->commandsToPatch.push_back({});
    EXPECT_ANY_THROW(commandQueue->patchCommands(*commandList, 0));
    commandList->commandsToPatch.clear();

    CFE_STATE destinationCfeStates[4];
    int32_t initialScratchAddress = 0x123400;
    for (size_t i = 0; i < 4; i++) {
        auto sourceCfeState = new CFE_STATE;
        *sourceCfeState = FamilyType::cmdInitCfeState;
        if constexpr (TestTraits<gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
            sourceCfeState->setNumberOfWalkers(2);
        }
        sourceCfeState->setMaximumNumberOfThreads(16);
        sourceCfeState->setScratchSpaceBuffer(initialScratchAddress);

        destinationCfeStates[i] = FamilyType::cmdInitCfeState;
        if constexpr (TestTraits<gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
            EXPECT_NE(destinationCfeStates[i].getNumberOfWalkers(), sourceCfeState->getNumberOfWalkers());
        }
        EXPECT_NE(destinationCfeStates[i].getMaximumNumberOfThreads(), sourceCfeState->getMaximumNumberOfThreads());

        CommandList::CommandToPatch commandToPatch;
        commandToPatch.pDestination = &destinationCfeStates[i];
        commandToPatch.pCommand = sourceCfeState;
        commandToPatch.type = CommandList::CommandToPatch::CommandType::FrontEndState;
        commandList->commandsToPatch.push_back(commandToPatch);
    }

    uint64_t patchedScratchAddress = 0xABCD00;
    commandQueue->patchCommands(*commandList, patchedScratchAddress);
    for (size_t i = 0; i < 4; i++) {
        EXPECT_EQ(patchedScratchAddress, destinationCfeStates[i].getScratchSpaceBuffer());
        auto &sourceCfeState = *reinterpret_cast<CFE_STATE *>(commandList->commandsToPatch[i].pCommand);
        if constexpr (TestTraits<gfxCoreFamily>::numberOfWalkersInCfeStateSupported) {
            EXPECT_EQ(destinationCfeStates[i].getNumberOfWalkers(), sourceCfeState.getNumberOfWalkers());
        }
        EXPECT_EQ(destinationCfeStates[i].getMaximumNumberOfThreads(), sourceCfeState.getMaximumNumberOfThreads());
        EXPECT_EQ(destinationCfeStates[i].getScratchSpaceBuffer(), sourceCfeState.getScratchSpaceBuffer());
    }
}

} // namespace ult
} // namespace L0
