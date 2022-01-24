/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/image/image_hw.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

using L0DebuggerTest = Test<L0DebuggerHwFixture>;

struct L0DebuggerInternalUsageTest : public L0DebuggerTest {
    void SetUp() override {
        VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
        defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
        L0DebuggerTest::SetUp();
    }
};

HWTEST_F(L0DebuggerInternalUsageTest, givenCommandListIsInititalizedThenByDefaultFlushTaskSubmissionEnabled) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    EXPECT_EQ(true, commandList->isFlushTaskSubmissionEnabled);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenFlushTaskSubmissionDisabledWhenCommandListIsInititalizedThenFlushTaskIsSetToFalse) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    EXPECT_EQ(false, commandList->isFlushTaskSubmissionEnabled);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenFlushTaskSubmissionEnabledWhenCommandListIsInititalizedOrResetThenCaptureSbaIsNotCalled) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    size_t usedSpaceBefore = 0;
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GE(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandList->commandContainer.getCommandStream()->getCpuBase(), usedSpaceAfter));

    auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->reset();
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenFlushTaskSubmissionDisabledWhenCommandListIsInititalizedOrResetThenCaptureSbaIsCalled) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    size_t usedSpaceBefore = 0;
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, commandList->commandContainer.getCommandStream()->getCpuBase(), usedSpaceAfter));

    auto sbaItor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->reset();
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenDebuggerLogsDisabledWhenCommandListIsSynchronizedThenSbaAddressesAreNotPrinted) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.DebuggerLogBitmask.set(0);

    EXPECT_NE(nullptr, device->getL0Debugger());
    testing::internal::CaptureStdout();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    commandList->executeCommandListImmediate(false);

    std::string output = testing::internal::GetCapturedStdout();
    size_t pos = output.find("Debugger: SBA");
    EXPECT_EQ(std::string::npos, pos);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendLaunchKernelThenSuccessIsReturned) {
    Mock<::L0::Kernel> kernel;
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledWithInternalCommandListForImmediateWhenAppendLaunchKernelThenSuccessIsReturned) {
    Mock<::L0::Kernel> kernel;
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendLaunchKernelThenSuccessIsReturned) {
    Mock<::L0::Kernel> kernel;

    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendPageFaultThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    size_t size = 0x100000001;
    NEO::MockGraphicsAllocation mockAllocationSrc(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x1234), size, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    NEO::MockGraphicsAllocation mockAllocationDst(0, NEO::GraphicsAllocation::AllocationType::INTERNAL_HOST_MEMORY,
                                                  reinterpret_cast<void *>(0x100003456), size, 0, sizeof(uint32_t),
                                                  MemoryPool::System4KBPages);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendPageFaultCopy(&mockAllocationDst, &mockAllocationSrc, size, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendEventResetThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue);

    result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
    context->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendEventResetWithTimestampThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue);

    result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
    context->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendEventResetWithTimestampThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue);

    result = commandList->appendEventReset(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
    context->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendSignalEventThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue);

    result = commandList->appendSignalEvent(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
    context->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendSignalEventWithTimestampThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue);

    result = commandList->appendSignalEvent(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
    context->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendSignalEventWithTimestampThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue);

    result = commandList->appendSignalEvent(event->toHandle());
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
    context->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendWaitOnEventWithTimestampThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue);

    ze_event_handle_t hEventHandle = event->toHandle();
    result = commandList->appendWaitOnEvents(1, &hEventHandle);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
    context->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendBarrierWithEventThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue);

    result = commandList->appendBarrier(event->toHandle(), 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
    context->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendBarrierWithTimestampEventThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue);

    result = commandList->appendBarrier(event->toHandle(), 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
    context->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForImmediateWhenAppendBarrierWithoutEventThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue);

    auto result = commandList->appendBarrier(nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionDisabledForImmediateWhenAppendBarrierWithEventThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Compute, returnValue);

    result = commandList->appendBarrier(event->toHandle(), 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
    context->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendLaunchKernelIndirectThenSuccessIsReturned) {
    Mock<::L0::Kernel> kernel;
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendLaunchKernelIndirect(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendLaunchKernelIndirectThenSuccessIsReturned) {
    Mock<::L0::Kernel> kernel;
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    ze_group_count_t groupCount{1, 1, 1};
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendLaunchKernelIndirect(kernel.toHandle(), &groupCount, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendMemoryCopyThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x100, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendMemoryCopyThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x100, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendMemoryCopyRegionThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendMemoryCopyRegion(dstPtr, &dr, 0, 0, srcPtr, &sr, 0, 0, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForRegularCommandListForAppendMemoryCopyRegionThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    uint32_t width = 16;
    uint32_t height = 16;
    ze_copy_region_t sr = {0U, 0U, 0U, width, height, 0U};
    ze_copy_region_t dr = {0U, 0U, 0U, width, height, 0U};

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whitebox_cast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    auto result = commandList->appendMemoryCopyRegion(dstPtr, &dr, 0, 0, srcPtr, &sr, 0, 0, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    commandList->destroy();
    commandQueue->destroy();
}

HWTEST_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendMemoryCopyRegionThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_copy_region_t dstRegion = {};
    ze_copy_region_t srcRegion = {};

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);

    auto result = commandList->appendMemoryCopyRegion(dstPtr, &dstRegion, 0, 0, srcPtr, &srcRegion, 0, 0, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendImageCopyRegionThenSuccessIsReturned, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    const ze_command_queue_desc_t queueDesc = {};
    bool internalEngine = false;

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &queueDesc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);

    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &desc);
    imageHWDst->initialize(device, &desc);

    returnValue = commandList0->appendImageCopy(imageHWDst->toHandle(), imageHWSrc->toHandle(), nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList0->appendImageCopyFromMemory(imageHWDst->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList0->appendImageCopyToMemory(dstPtr, imageHWSrc->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

HWTEST2_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendImageCopyRegionThenSuccessIsReturned, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    const ze_command_queue_desc_t queueDesc = {};
    bool internalEngine = true;

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &queueDesc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::Copy,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(commandList0->cmdQImmediate);
    if (neoDevice->getInternalCopyEngine()) {
        EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalCopyEngine()->commandStreamReceiver);
    } else {
        EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);
    }

    ze_image_desc_t desc = {};
    desc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
    desc.type = ZE_IMAGE_TYPE_3D;
    desc.format.layout = ZE_IMAGE_FORMAT_LAYOUT_8_8_8_8;
    desc.format.type = ZE_IMAGE_FORMAT_TYPE_UINT;
    desc.width = 11;
    desc.height = 13;
    desc.depth = 17;

    desc.format.x = ZE_IMAGE_FORMAT_SWIZZLE_A;
    desc.format.y = ZE_IMAGE_FORMAT_SWIZZLE_0;
    desc.format.z = ZE_IMAGE_FORMAT_SWIZZLE_1;
    desc.format.w = ZE_IMAGE_FORMAT_SWIZZLE_X;
    auto imageHWSrc = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    auto imageHWDst = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
    imageHWSrc->initialize(device, &desc);
    imageHWDst->initialize(device, &desc);

    returnValue = commandList0->appendImageCopy(imageHWDst->toHandle(), imageHWSrc->toHandle(), nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList0->appendImageCopyFromMemory(imageHWDst->toHandle(), srcPtr, nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList0->appendImageCopyToMemory(dstPtr, imageHWSrc->toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
}

HWTEST2_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionEnabledCommandListAndAppendMemoryCopyCalledInLoopThenMultipleCommandBufferAreUsedAndSuccessIsReturned, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);
    ASSERT_NE(nullptr, commandList);

    for (uint32_t count = 0; count < 2048; count++) {
        auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x100, nullptr, 0, nullptr);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }
    commandList->destroy();
}

HWTEST2_F(L0DebuggerInternalUsageTest, givenUseCsrImmediateSubmissionDisabledCommandListAndAppendMemoryCopyCalledInLoopThenMultipleCommandBufferAreUsedAndSuccessIsReturned, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);
    ASSERT_NE(nullptr, commandList);

    for (uint32_t count = 0; count < 2048; count++) {
        auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 0x100, nullptr, 0, nullptr);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    }
    commandList->destroy();
}

HWTEST2_F(L0DebuggerInternalUsageTest, givenDebuggingEnabledWhenInternalCmdQIsUsedThenDebuggerPathsAreNotExecuted, IsAtLeastSkl) {
    ze_command_queue_desc_t queueDesc = {};

    std::unique_ptr<MockCommandQueueHw<gfxCoreFamily>, Deleter> commandQueue(new MockCommandQueueHw<gfxCoreFamily>(device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc));
    commandQueue->initialize(false, true);
    EXPECT_TRUE(commandQueue->internalUsage);
    ze_result_t returnValue;
    ze_command_list_handle_t commandLists[] = {
        CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto sbaBuffer = device->getL0Debugger()->getSbaTrackingBuffer(neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().getContextId());
    auto sipIsa = NEO::SipKernel::getSipKernel(*neoDevice).getSipAllocation();
    auto debugSurface = device->getDebugSurface();
    bool sbaFound = false;
    bool sipFound = false;
    bool debugSurfaceFound = false;

    for (auto iter : commandQueue->residencyContainerSnapshot) {
        if (iter == sbaBuffer) {
            sbaFound = true;
        }
        if (iter == sipIsa) {
            sipFound = true;
        }
        if (iter == debugSurface) {
            debugSurfaceFound = true;
        }
    }
    EXPECT_FALSE(sbaFound);
    EXPECT_FALSE(sipFound);
    EXPECT_FALSE(debugSurfaceFound);

    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->captureStateBaseAddressCount);
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->programSbaTrackingCommandsCount);
    EXPECT_EQ(0u, getMockDebuggerL0Hw<FamilyType>()->getSbaTrackingCommandsSizeCount);

    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->destroy();
}

} // namespace ult
} // namespace L0
