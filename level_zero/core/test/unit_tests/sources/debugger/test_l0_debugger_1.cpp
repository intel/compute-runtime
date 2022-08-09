/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

using L0DebuggerTest = Test<L0DebuggerHwFixture>;
using L0DebuggerParameterizedTests = L0DebuggerHwParameterizedFixture;

TEST_F(L0DebuggerTest, givenL0DebuggerWhenCallingIsLegacyThenFalseIsReturned) {
    EXPECT_FALSE(neoDevice->getDebugger()->isLegacy());
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingSourceLevelDebuggerThenNullptrReturned) {
    EXPECT_EQ(nullptr, neoDevice->getSourceLevelDebugger());
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingL0DebuggerThenValidDebuggerInstanceIsReturned) {
    EXPECT_NE(nullptr, device->getL0Debugger());
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingSipAllocationThenValidSipTypeIsReturned) {
    auto systemRoutine = SipKernel::getSipKernel(*neoDevice).getSipAllocation();
    ASSERT_NE(nullptr, systemRoutine);

    auto sipType = SipKernel::getSipKernelType(*neoDevice);
    auto isHexadecimalArrayPreferred = HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSipKernelAsHexadecimalArrayPreferred();

    auto expectedSipAllocation = isHexadecimalArrayPreferred
                                     ? NEO::MockSipData::mockSipKernel->getSipAllocation()
                                     : neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getSipAllocation();

    EXPECT_EQ(expectedSipAllocation, systemRoutine);
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingSipTypeThenDebugBindlessIsReturned) {
    auto sipType = SipKernel::getSipKernelType(*neoDevice);
    EXPECT_EQ(NEO::SipKernelType::DbgBindless, sipType);
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingStateSaveAreaHeaderThenValidSipTypeIsReturned) {
    auto &stateSaveAreaHeader = SipKernel::getSipKernel(*neoDevice).getStateSaveAreaHeader();

    auto sipType = SipKernel::getSipKernelType(*neoDevice);
    auto &expectedStateSaveAreaHeader = neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getStateSaveAreaHeader();

    EXPECT_EQ(expectedStateSaveAreaHeader, stateSaveAreaHeader);
}

TEST_F(L0DebuggerTest, givenProgramDebuggingEnabledWhenDebuggerIsCreatedThenFusedEusAreDisabled) {
    EXPECT_TRUE(driverHandle->enableProgramDebugging);
    EXPECT_FALSE(neoDevice->getHardwareInfo().capabilityTable.fusedEuEnabled);
}

TEST_F(L0DebuggerTest, givenProgramDebuggingEnabledWhenDebuggerIsCreatedThenCompressionIsDisabled) {
    EXPECT_TRUE(driverHandle->enableProgramDebugging);
    EXPECT_FALSE(neoDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(neoDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedImages);
}

using L0DebuggerPerContextAddressSpaceTest = Test<L0DebuggerPerContextAddressSpaceFixture>;
HWTEST_F(L0DebuggerPerContextAddressSpaceTest, givenDebuggingEnabledWhenCommandListIsExecutedThenValidKernelDebugCommandsAreAdded) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    auto usedSpaceBefore = commandQueue->commandStream->getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream->getCpuBase(), 0), usedSpaceAfter));

    auto miLoadImm = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());

    size_t debugModeRegisterCount = 0;
    size_t tdDebugControlRegisterCount = 0;

    for (size_t i = 0; i < miLoadImm.size(); i++) {
        MI_LOAD_REGISTER_IMM *miLoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*miLoadImm[i]);
        ASSERT_NE(nullptr, miLoad);

        if (miLoad->getRegisterOffset() == DebugModeRegisterOffset<FamilyType>::registerOffset) {
            EXPECT_EQ(DebugModeRegisterOffset<FamilyType>::debugEnabledValue, miLoad->getDataDword());
            debugModeRegisterCount++;
        }
        if (miLoad->getRegisterOffset() == TdDebugControlRegisterOffset<FamilyType>::registerOffset) {
            EXPECT_EQ(TdDebugControlRegisterOffset<FamilyType>::debugEnabledValue, miLoad->getDataDword());
            tdDebugControlRegisterCount++;
        }
    }
    // those register should not be used
    EXPECT_EQ(0u, debugModeRegisterCount);
    EXPECT_EQ(0u, tdDebugControlRegisterCount);

    if (!HwHelper::get(hwInfo.platform.eRenderCoreFamily).isSipWANeeded(hwInfo)) {
        auto stateSipCmds = findAll<STATE_SIP *>(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, stateSipCmds.size());

        STATE_SIP *stateSip = genCmdCast<STATE_SIP *>(*stateSipCmds[0]);

        auto systemRoutine = SipKernel::getSipKernel(*neoDevice).getSipAllocation();
        ASSERT_NE(nullptr, systemRoutine);
        EXPECT_EQ(systemRoutine->getGpuAddressToPatch(), stateSip->getSystemInstructionPointer());
    }

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

using Gen12Plus = IsAtLeastGfxCore<IGFX_GEN12_CORE>;

HWTEST2_P(L0DebuggerParameterizedTests, givenDebuggerWhenAppendingKernelToCommandListThenBindlessSurfaceStateForDebugSurfaceIsProgrammedAtOffsetZero, Gen12Plus) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    Mock<::L0::Kernel> kernel;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->close();

    auto *ssh = commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);

    auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ssh->getCpuBase());
    auto debugSurface = static_cast<L0::DeviceImp *>(device)->getDebugSurface();

    SURFACE_STATE_BUFFER_LENGTH length;
    length.Length = static_cast<uint32_t>(debugSurface->getUnderlyingBufferSize() - 1);

    EXPECT_EQ(length.SurfaceState.Depth + 1u, debugSurfaceState->getDepth());
    EXPECT_EQ(length.SurfaceState.Width + 1u, debugSurfaceState->getWidth());
    EXPECT_EQ(length.SurfaceState.Height + 1u, debugSurfaceState->getHeight());
    EXPECT_EQ(debugSurface->getGpuAddress(), debugSurfaceState->getSurfaceBaseAddress());

    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, debugSurfaceState->getSurfaceType());
    EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, debugSurfaceState->getCoherencyType());
}

HWTEST2_P(L0DebuggerParameterizedTests, givenDebuggerWhenAppendingKernelToCommandListThenDebugSurfaceIsProgrammedWithL3DisabledMOCS, Gen12Plus) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    Mock<::L0::Kernel> kernel;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), &groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->close();

    auto *ssh = commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
    auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ssh->getCpuBase());

    const auto mocsNoCache = device->getNEODevice()->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
    const auto actualMocs = debugSurfaceState->getMemoryObjectControlState();

    EXPECT_EQ(actualMocs, mocsNoCache);
}

HWTEST_P(L0DebuggerParameterizedTests, givenEnabledDebuggingWhenIsaTypeAllocatedOnMultitileDeviceThenSharedAllocationIsCreated) {
    auto debugger = device->getL0Debugger();
    ASSERT_NE(nullptr, debugger);

    size_t kernelIsaSize = 0x1000;
    NEO::AllocationType allocTypes[] = {NEO::AllocationType::KERNEL_ISA,
                                        NEO::AllocationType::KERNEL_ISA_INTERNAL,
                                        NEO::AllocationType::DEBUG_MODULE_AREA};

    for (int i = 0; i < 3; i++) {
        auto allocation = device->getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(
            {neoDevice->getRootDeviceIndex(), kernelIsaSize, allocTypes[i], neoDevice->getDeviceBitfield()});

        ASSERT_NE(nullptr, allocation);
        EXPECT_EQ(1u, allocation->storageInfo.getNumBanks());

        EXPECT_TRUE(allocation->storageInfo.cloningOfPageTables);
        EXPECT_FALSE(allocation->storageInfo.tileInstanced);

        device->getNEODevice()->getMemoryManager()->freeGraphicsMemory(allocation);
    }
}

using L0DebuggerSimpleTest = Test<DeviceFixture>;

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledWithImmediateCommandListToInvokeNonKernelOperationsThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> eventObject(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, eventObject->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csr);

    returnValue = commandList->appendWaitOnEvents(1, &event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendBarrier(nullptr, 1, &event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendSignalEvent(event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = eventObject->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(eventObject->queryStatus(), ZE_RESULT_SUCCESS);

    returnValue = commandList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(dstPtr), nullptr, 0, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendEventReset(event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    context->freeMem(dstPtr);
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionDisabledWithImmediateCommandListToInvokeNonKernelOperationsThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::RenderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->device);
    EXPECT_EQ(1u, commandList->cmdListType);
    EXPECT_NE(nullptr, commandList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<L0::Event> eventObject(L0::Event::fromHandle(event));
    ASSERT_NE(nullptr, eventObject->csr);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csr);

    returnValue = commandList->appendWaitOnEvents(1, &event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendBarrier(nullptr, 1, &event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendSignalEvent(event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = eventObject->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(eventObject->queryStatus(), ZE_RESULT_SUCCESS);

    returnValue = commandList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(dstPtr), nullptr, 0, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendEventReset(event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    context->freeMem(dstPtr);
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendMemoryFillWithDeviceMemoryThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->freeMem(dstPtr);
    commandList->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendMemoryFillThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->freeMem(dstPtr);
    commandList->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionDisabledForImmediateCommandListForAppendMemoryFillThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);

    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->freeMem(dstPtr);
    commandList->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledForRegularCommandListForAppendMemoryFillThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;

    auto commandList = CommandList::fromHandle(commandLists[0]);
    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    context->freeMem(dstPtr);
    commandList->destroy();

    commandQueue->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionDisabledForRegularCommandListForAppendMemoryFillThenSuccessIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue->commandStream);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;

    auto commandList = CommandList::fromHandle(commandLists[0]);
    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    context->freeMem(dstPtr);
    commandList->destroy();

    commandQueue->destroy();
}

HWTEST2_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledCommandListAndAppendPageFaultCopyThenSuccessIsReturned, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(true);

    size_t size = (sizeof(uint32_t) * 4);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);
    ASSERT_NE(nullptr, commandList);

    NEO::GraphicsAllocation srcPtr(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                   reinterpret_cast<void *>(0x1234), size, 0, sizeof(uint32_t),
                                   MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    NEO::GraphicsAllocation dstPtr(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                   reinterpret_cast<void *>(0x2345), size, 0, sizeof(uint32_t),
                                   MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);

    auto result = commandList->appendPageFaultCopy(&dstPtr, &srcPtr, 0x100, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionDisabledCommandListAndAppendPageFaultCopyThenSuccessIsReturned, IsAtLeastSkl) {
    DebugManagerStateRestore restorer;
    NEO::DebugManager.flags.EnableFlushTaskSubmission.set(false);

    size_t size = (sizeof(uint32_t) * 4);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::RenderCompute, returnValue);
    ASSERT_NE(nullptr, commandList);

    NEO::GraphicsAllocation srcPtr(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                   reinterpret_cast<void *>(0x1234), size, 0, sizeof(uint32_t),
                                   MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);
    NEO::GraphicsAllocation dstPtr(0, NEO::AllocationType::INTERNAL_HOST_MEMORY,
                                   reinterpret_cast<void *>(0x2345), size, 0, sizeof(uint32_t),
                                   MemoryPool::System4KBPages, MemoryManager::maxOsContextCount);

    auto result = commandList->appendPageFaultCopy(&dstPtr, &srcPtr, 0x100, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_F(L0DebuggerTest, givenNotXeHpOrXeHpgCoreAndDebugIsActiveThenDisableL3CacheInGmmHelperIsNotSet, IsNotXeHpOrXeHpgCore) {
    EXPECT_FALSE(static_cast<MockGmmHelper *>(neoDevice->getGmmHelper())->allResourcesUncached);
}

HWTEST2_F(L0DebuggerTest, givenXeHpOrXeHpgCoreAndDebugIsActiveThenDisableL3CacheInGmmHelperIsSet, IsXeHpOrXeHpgCore) {
    EXPECT_TRUE(static_cast<MockGmmHelper *>(neoDevice->getGmmHelper())->allResourcesUncached);
}

INSTANTIATE_TEST_CASE_P(SBAModesForDebugger, L0DebuggerParameterizedTests, ::testing::Values(0, 1));

} // namespace ult
} // namespace L0
