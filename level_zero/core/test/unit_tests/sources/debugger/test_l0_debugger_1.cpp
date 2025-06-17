/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_gmm_helper.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_memory_copy_params.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

using L0DebuggerTest = Test<L0DebuggerHwFixture>;
using L0DebuggerParameterizedTests = L0DebuggerHwParameterizedFixture;
using L0DebuggerGlobalStatelessTest = Test<L0DebuggerHwGlobalStatelessFixture>;

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingL0DebuggerThenValidDebuggerInstanceIsReturned) {
    EXPECT_NE(nullptr, device->getL0Debugger());
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingSipAllocationThenValidSipTypeIsReturned) {
    auto systemRoutine = SipKernel::getSipKernel(*neoDevice, nullptr).getSipAllocation();
    ASSERT_NE(nullptr, systemRoutine);

    auto sipType = SipKernel::getSipKernelType(*neoDevice);
    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    auto isHexadecimalArrayPreferred = gfxCoreHelper.isSipKernelAsHexadecimalArrayPreferred();

    auto expectedSipAllocation = isHexadecimalArrayPreferred
                                     ? NEO::MockSipData::mockSipKernel->getSipAllocation()
                                     : neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getSipAllocation();

    EXPECT_EQ(expectedSipAllocation, systemRoutine);
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingSipTypeThenDebugBindlessIsReturned) {
    auto &compilerProductHelper = neoDevice->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);

    auto sipType = SipKernel::getSipKernelType(*neoDevice);
    if (heaplessEnabled) {
        EXPECT_EQ(NEO::SipKernelType::dbgHeapless, sipType);

    } else {
        EXPECT_EQ(NEO::SipKernelType::dbgBindless, sipType);
    }
}

TEST_F(L0DebuggerTest, givenL0DebuggerWhenGettingStateSaveAreaHeaderThenValidSipTypeIsReturned) {
    auto &stateSaveAreaHeader = SipKernel::getSipKernel(*neoDevice, nullptr).getStateSaveAreaHeader();

    auto sipType = SipKernel::getSipKernelType(*neoDevice);
    auto &expectedStateSaveAreaHeader = neoDevice->getBuiltIns()->getSipKernel(sipType, *neoDevice).getStateSaveAreaHeader();

    EXPECT_EQ(expectedStateSaveAreaHeader, stateSaveAreaHeader);
}

TEST_F(L0DebuggerTest, givenProgramDebuggingEnabledWhenDebuggerIsCreatedThenFusedEusAreDisabled) {
    EXPECT_TRUE(driverHandle->enableProgramDebugging == NEO::DebuggingMode::online);
    EXPECT_FALSE(neoDevice->getHardwareInfo().capabilityTable.fusedEuEnabled);
}

TEST_F(L0DebuggerTest, givenProgramDebuggingEnabledWhenDebuggerIsCreatedThenCompressionIsDisabled) {
    EXPECT_TRUE(driverHandle->enableProgramDebugging == NEO::DebuggingMode::online);
    EXPECT_FALSE(neoDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(neoDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedImages);
}

using L0DebuggerPerContextAddressSpaceTest = Test<L0DebuggerPerContextAddressSpaceFixture>;
HWTEST_F(L0DebuggerPerContextAddressSpaceTest, givenDebuggingEnabledWhenCommandListIsExecutedThenValidKernelDebugCommandsAreAdded) {

    auto &compilerProductHelper = neoDevice->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    if (compilerProductHelper.isHeaplessStateInitEnabled(heaplessEnabled)) {
        GTEST_SKIP();
    }

    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->close();
    }

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

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

    auto stateSipCmds = findAll<STATE_SIP *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, stateSipCmds.size());

    STATE_SIP *stateSip = genCmdCast<STATE_SIP *>(*stateSipCmds[0]);

    auto systemRoutine = SipKernel::getSipKernel(*neoDevice, nullptr).getSipAllocation();
    ASSERT_NE(nullptr, systemRoutine);
    EXPECT_EQ(systemRoutine->getGpuAddressToPatch(), stateSip->getSystemInstructionPointer());

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(commandLists[i]);
        commandList->destroy();
    }

    commandQueue->destroy();
}

HWTEST_F(L0DebuggerPerContextAddressSpaceTest, givenDebuggingEnabledWhenTwoCommandQueuesExecuteCommandListThenSipIsDispatchedOncePerContext) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    auto &compilerProductHelper = neoDevice->getCompilerProductHelper();
    auto heaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    if (compilerProductHelper.isHeaplessStateInitEnabled(heaplessEnabled)) {
        GTEST_SKIP();
    }

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;

    auto &defaultEngine = neoDevice->getDefaultEngine();

    defaultEngine.commandStreamReceiver->setPreemptionMode(NEO::PreemptionMode::ThreadGroup);

    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, defaultEngine.commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto commandQueue2 = whiteboxCast(CommandQueue::create(productFamily, device, defaultEngine.commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue2);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto engineGroupType = device->getGfxCoreHelper().getEngineGroupType(defaultEngine.getEngineType(), defaultEngine.getEngineUsage(), neoDevice->getHardwareInfo());
    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, engineGroupType, 0u, returnValue, false)->toHandle()};
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto commandList = CommandList::fromHandle(commandLists[0]);
    commandList->close();

    returnValue = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto usedSpaceAfter = commandQueue->commandStream.getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        commandQueue->commandStream.getCpuBase(),
        usedSpaceAfter));

    auto stateSipCmds = findAll<STATE_SIP *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(1u, stateSipCmds.size());

    returnValue = commandQueue2->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto usedSpaceAfter2 = commandQueue2->commandStream.getUsed();

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        commandQueue2->commandStream.getCpuBase(),
        usedSpaceAfter2));

    stateSipCmds = findAll<STATE_SIP *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, stateSipCmds.size());

    commandList->destroy();
    commandQueue->destroy();
    commandQueue2->destroy();
}

using Gen12Plus = IsAtLeastGfxCore<IGFX_GEN12_CORE>;

HWTEST2_P(L0DebuggerParameterizedTests, givenDebuggerWhenAppendingKernelToCommandListThenBindlessSurfaceStateForDebugSurfaceIsProgrammedAtOffsetZero, Gen12Plus) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    auto bindlessHeapsHelper = device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.get();

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->close();

    NEO::IndirectHeap *ssh;
    if (bindlessHeapsHelper) {
        ssh = neoDevice->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::specialSsh);
    } else {
        ssh = commandList->getCmdContainer().getIndirectHeap(NEO::HeapType::surfaceState);
    }

    auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ssh->getCpuBase());
    auto debugSurface = static_cast<L0::DeviceImp *>(device)->getDebugSurface();

    SurfaceStateBufferLength length;
    length.length = static_cast<uint32_t>(debugSurface->getUnderlyingBufferSize() - 1);

    EXPECT_EQ(length.surfaceState.depth + 1u, debugSurfaceState->getDepth());
    EXPECT_EQ(length.surfaceState.width + 1u, debugSurfaceState->getWidth());
    EXPECT_EQ(length.surfaceState.height + 1u, debugSurfaceState->getHeight());
    EXPECT_EQ(debugSurface->getGpuAddress(), debugSurfaceState->getSurfaceBaseAddress());

    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER, debugSurfaceState->getSurfaceType());
    if constexpr (IsAtMostXeCore::isMatched<productFamily>()) {
        EXPECT_EQ(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT, debugSurfaceState->getCoherencyType());
    }
}

HWTEST2_P(L0DebuggerParameterizedTests, givenDebuggerWhenAppendingKernelToCommandListThenDebugSurfaceIsProgrammedWithL3DisabledMOCS, Gen12Plus) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    auto bindlessHeapsHelper = device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.get();

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->close();

    NEO::IndirectHeap *ssh;
    if (bindlessHeapsHelper) {
        ssh = neoDevice->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::specialSsh);
    } else {
        ssh = commandList->getCmdContainer().getIndirectHeap(NEO::HeapType::surfaceState);
    }

    auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ssh->getCpuBase());

    const auto mocsNoCache = device->getNEODevice()->getGmmHelper()->getUncachedMOCS();
    const auto actualMocs = debugSurfaceState->getMemoryObjectControlState();

    EXPECT_EQ(actualMocs, mocsNoCache);
}

HWTEST_P(L0DebuggerParameterizedTests, givenEnabledDebuggingWhenIsaTypeAllocatedOnMultitileDeviceThenSharedAllocationIsCreated) {
    auto debugger = device->getL0Debugger();
    ASSERT_NE(nullptr, debugger);

    size_t kernelIsaSize = 0x1000;
    NEO::AllocationType allocTypes[] = {NEO::AllocationType::kernelIsa,
                                        NEO::AllocationType::kernelIsaInternal,
                                        NEO::AllocationType::debugModuleArea};

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
    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

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

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    returnValue = commandList->appendWaitOnEvents(1, &event, nullptr, false, true, false, false, false, false);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendBarrier(nullptr, 1, &event, false);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendSignalEvent(event, false);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = eventObject->hostSignal(false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(eventObject->queryStatus(), ZE_RESULT_SUCCESS);

    returnValue = commandList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(dstPtr), nullptr, 0, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    returnValue = commandList->appendEventReset(event);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);

    context->freeMem(dstPtr);
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendMemoryFillWithDeviceMemoryThenSuccessIsReturned) {

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::renderCompute, returnValue);
    CmdListMemoryCopyParams copyParams = {};
    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->freeMem(dstPtr);
    commandList->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledForImmediateCommandListForAppendMemoryFillThenSuccessIsReturned) {
    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::renderCompute, returnValue);
    CmdListMemoryCopyParams copyParams = {};
    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    context->freeMem(dstPtr);
    commandList->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledForRegularCommandListForAppendMemoryFillThenSuccessIsReturned) {
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    const uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    void *dstPtr = nullptr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &dstPtr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    int pattern = 1;
    CmdListMemoryCopyParams copyParams = {};
    auto commandList = CommandList::fromHandle(commandLists[0]);
    result = commandList->appendMemoryFill(dstPtr, reinterpret_cast<void *>(&pattern), sizeof(pattern), 4096u, nullptr, 0, nullptr, copyParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    commandList->close();

    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandQueue->synchronize(0);

    context->freeMem(dstPtr);
    commandList->destroy();

    commandQueue->destroy();
}

HWTEST_F(L0DebuggerSimpleTest, givenUseCsrImmediateSubmissionEnabledCommandListAndAppendPageFaultCopyThenSuccessIsReturned) {
    size_t size = (sizeof(uint32_t) * 4);
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, true, NEO::EngineGroupType::renderCompute, returnValue);
    ASSERT_NE(nullptr, commandList);

    NEO::GraphicsAllocation srcPtr(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                   reinterpret_cast<void *>(0x1234), size, 0, sizeof(uint32_t),
                                   MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);
    NEO::GraphicsAllocation dstPtr(0, 1u /*num gmms*/, NEO::AllocationType::internalHostMemory,
                                   reinterpret_cast<void *>(0x2345), size, 0, sizeof(uint32_t),
                                   MemoryPool::system4KBPages, MemoryManager::maxOsContextCount);

    auto result = commandList->appendPageFaultCopy(&dstPtr, &srcPtr, 0x100, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->destroy();
}

HWTEST2_F(L0DebuggerTest, givenDebuggerEnabledAndL1CachePolicyWBWhenAppendingThenDebugSurfaceHasCachePolicyWBP, IsAtLeastXeCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    DebugManagerStateRestore restore;
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    auto bindlessHeapsHelper = device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.get();

    NEO::RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(*device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]);
    raii.mockProductHelper->returnedL1CachePolicy = RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB;
    raii.mockProductHelper->returnedL1CachePolicyIfDebugger = RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP;

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    returnValue = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    commandList->close();

    NEO::IndirectHeap *ssh;
    if (bindlessHeapsHelper) {
        ssh = neoDevice->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::specialSsh);
    } else {
        ssh = commandList->getCmdContainer().getIndirectHeap(NEO::HeapType::surfaceState);
    }

    ASSERT_NE(ssh, nullptr);
    auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(ssh->getCpuBase());
    ASSERT_NE(debugSurfaceState, nullptr);
    auto debugSurface = static_cast<L0::DeviceImp *>(device)->getDebugSurface();
    ASSERT_NE(debugSurface, nullptr);
    ASSERT_EQ(debugSurface->getGpuAddress(), debugSurfaceState->getSurfaceBaseAddress());
    EXPECT_EQ(debugSurfaceState->getL1CacheControlCachePolicy(), RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP);
}

HWTEST2_F(L0DebuggerTest, givenNotXeHpOrXeHpgCoreAndDebugIsActiveThenDisableL3CacheInGmmHelperIsNotSet, IsNotXeHpgCore) {
    EXPECT_FALSE(static_cast<MockGmmHelper *>(neoDevice->getGmmHelper())->allResourcesUncached);
}

HWTEST2_F(L0DebuggerTest, givenDebugIsActiveThenDisableL3CacheInGmmHelperIsSet, IsDG2) {
    bool disableL3CacheForDebug = neoDevice->getProductHelper().disableL3CacheForDebug(neoDevice->getHardwareInfo());
    EXPECT_EQ(disableL3CacheForDebug, static_cast<MockGmmHelper *>(neoDevice->getGmmHelper())->allResourcesUncached);
}

INSTANTIATE_TEST_SUITE_P(SBAModesForDebugger, L0DebuggerParameterizedTests, ::testing::Values(0, 1));

struct MockKernelImmutableData : public KernelImmutableData {
    using KernelImmutableData::isaGraphicsAllocation;
    using KernelImmutableData::kernelDescriptor;
    using KernelImmutableData::kernelInfo;

    MockKernelImmutableData(L0::Device *device) : KernelImmutableData(device) {}
};

HWTEST_F(L0DebuggerTest, givenFlushTaskSubmissionAndSharedHeapsEnabledWhenAppendingKernelUsingNewHeapThenDebugSurfaceIsProgrammedOnce) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.EnableImmediateCmdListHeapSharing.set(1);
    NEO::debugManager.flags.UseImmediateFlushTask.set(0);
    NEO::debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    auto bindlessHeapsHelper = device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.get();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_TRUE(commandList->immediateCmdListHeapSharing);

    auto kernelInfo = std::make_unique<NEO::KernelInfo>();
    auto kernelDescriptor = std::make_unique<NEO::KernelDescriptor>();
    auto kernelImmData = std::make_unique<MockKernelImmutableData>(device);

    kernelImmData->kernelInfo = kernelInfo.get();
    kernelImmData->kernelDescriptor = kernelDescriptor.get();
    kernelImmData->isaGraphicsAllocation.reset(new MockGraphicsAllocation());

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    kernel.kernelImmData = kernelImmData.get();

    CmdListKernelLaunchParams launchParams = {};
    ze_group_count_t groupCount{1, 1, 1};
    returnValue = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    NEO::IndirectHeap *csrHeap;
    if (bindlessHeapsHelper) {
        csrHeap = neoDevice->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::specialSsh);
    } else {
        csrHeap = &commandList->getCsr(false)->getIndirectHeap(NEO::HeapType::surfaceState, 0);
    }

    ASSERT_NE(nullptr, csrHeap);

    auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(csrHeap->getCpuBase());
    ASSERT_NE(debugSurfaceState, nullptr);
    auto debugSurface = static_cast<::L0::DeviceImp *>(device)->getDebugSurface();
    ASSERT_NE(debugSurface, nullptr);
    ASSERT_EQ(debugSurface->getGpuAddress(), debugSurfaceState->getSurfaceBaseAddress());

    memset(debugSurfaceState, 0, sizeof(*debugSurfaceState));

    returnValue = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    if (!commandList->isHeaplessStateInitEnabled()) {
        ASSERT_EQ(0u, debugSurfaceState->getSurfaceBaseAddress());
    }

    kernelImmData->isaGraphicsAllocation.reset(nullptr);
    commandList->destroy();
}

HWTEST2_F(L0DebuggerTest, givenImmediateFlushTaskWhenAppendingKernelUsingNewHeapThenDebugSurfaceIsProgrammedOnce, IsAtLeastXeCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    DebugManagerStateRestore restorer;

    NEO::debugManager.flags.UseImmediateFlushTask.set(1);
    NEO::debugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::privateHeaps));

    auto bindlessHeapsHelper = device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.get();

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto commandList = CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::compute, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto kernelInfo = std::make_unique<NEO::KernelInfo>();
    auto kernelDescriptor = std::make_unique<NEO::KernelDescriptor>();
    auto kernelImmData = std::make_unique<MockKernelImmutableData>(device);

    kernelImmData->kernelInfo = kernelInfo.get();
    kernelImmData->kernelDescriptor = kernelDescriptor.get();
    kernelImmData->isaGraphicsAllocation.reset(new MockGraphicsAllocation());

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    kernel.kernelImmData = kernelImmData.get();

    CmdListKernelLaunchParams launchParams = {};
    ze_group_count_t groupCount{1, 1, 1};
    returnValue = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    NEO::IndirectHeap *csrHeap;
    if (bindlessHeapsHelper) {
        csrHeap = neoDevice->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::specialSsh);
    } else {
        csrHeap = &commandList->getCsr(false)->getIndirectHeap(NEO::HeapType::surfaceState, 0);
    }

    ASSERT_NE(nullptr, csrHeap);

    auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(csrHeap->getCpuBase());
    ASSERT_NE(debugSurfaceState, nullptr);
    auto debugSurface = static_cast<::L0::DeviceImp *>(device)->getDebugSurface();
    ASSERT_NE(debugSurface, nullptr);
    ASSERT_EQ(debugSurface->getGpuAddress(), debugSurfaceState->getSurfaceBaseAddress());

    memset(debugSurfaceState, 0, sizeof(*debugSurfaceState));

    returnValue = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ASSERT_EQ(0u, debugSurfaceState->getSurfaceBaseAddress());

    kernelImmData->isaGraphicsAllocation.reset(nullptr);
    commandList->destroy();
}
struct DebuggerWithGlobalBindlessFixture : public L0DebuggerFixture {
    void setUp() {
        NEO::debugManager.flags.UseExternalAllocatorForSshAndDsh.set(true);
        L0DebuggerFixture::setUp(false);

        auto mockHelper = std::make_unique<MockBindlesHeapsHelper>(neoDevice,
                                                                   neoDevice->getNumGenericSubDevices() > 1);
        mockHelper->globalBindlessDsh = false;
        bindlessHelper = mockHelper.get();

        neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(mockHelper.release());

        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->enableProgramDebugging = NEO::DebuggingMode::online;

        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void tearDown() {
        L0DebuggerFixture::tearDown();
    }
    DebugManagerStateRestore restorer;
    MockBindlesHeapsHelper *bindlessHelper = nullptr;
};

using DebuggerWithGlobalBindlessTest = Test<DebuggerWithGlobalBindlessFixture>;

HWTEST_F(DebuggerWithGlobalBindlessTest, GivenGlobalBindlessHeapWhenDeviceIsCreatedThenDebugSurfaceStateIsProgrammedAtBindlessOffsetZero) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto globalBindlessBase = bindlessHelper->getGlobalHeapsBase();

    auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(bindlessHelper->getHeap(NEO::BindlessHeapsHelper::specialSsh)->getCpuBase());
    auto debugSurface = static_cast<L0::DeviceImp *>(device)->getDebugSurface();

    EXPECT_EQ(globalBindlessBase, bindlessHelper->getHeap(NEO::BindlessHeapsHelper::specialSsh)->getHeapGpuBase());

    SurfaceStateBufferLength length;
    length.length = static_cast<uint32_t>(debugSurface->getUnderlyingBufferSize() - 1);

    EXPECT_EQ(length.surfaceState.depth + 1u, debugSurfaceState->getDepth());
    EXPECT_EQ(length.surfaceState.width + 1u, debugSurfaceState->getWidth());
    EXPECT_EQ(length.surfaceState.height + 1u, debugSurfaceState->getHeight());
    EXPECT_EQ(debugSurface->getGpuAddress(), debugSurfaceState->getSurfaceBaseAddress());
}

HWTEST_F(DebuggerWithGlobalBindlessTest, GivenGlobalBindlessHeapWhenAppendingKernelToCmdListThenCmdContainerSshIsNotUsedForDebugSurface) {
    DebugManagerStateRestore restore;
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();
    kernel.descriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;
    kernel.descriptor.kernelAttributes.imageAddressingMode = NEO::KernelDescriptor::Bindless;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->close();

    auto ssh = commandList->getCmdContainer().getIndirectHeap(NEO::HeapType::surfaceState);

    EXPECT_EQ(nullptr, ssh);
}

HWTEST_F(DebuggerWithGlobalBindlessTest, GivenGlobalBindlessHeapWhenExecutingCmdListThenSpecialSshIsResident) {
    DebugManagerStateRestore restore;
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    ze_command_list_handle_t commandLists[] = {
        CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto commandList = CommandList::fromHandle(commandLists[0]);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();

    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    commandList->close();

    memoryOperationsHandler->captureGfxAllocationsForMakeResident = true;
    result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto allocIter = std::find(memoryOperationsHandler->gfxAllocationsForMakeResident.begin(),
                               memoryOperationsHandler->gfxAllocationsForMakeResident.end(),
                               bindlessHelper->getHeap(NEO::BindlessHeapsHelper::specialSsh)->getGraphicsAllocation());
    EXPECT_EQ(memoryOperationsHandler->gfxAllocationsForMakeResident.end(), allocIter);
    commandList->destroy();
    commandQueue->destroy();
}

HWTEST_F(DebuggerWithGlobalBindlessTest, GivenGlobalBindlessHeapWhenAppendingToImmCmdListThenSpecialSshIsResident) {
    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;

    DebugManagerStateRestore restore;
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    ze_command_list_handle_t commandListHandle = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::renderCompute, returnValue);
    auto commandList = CommandList::fromHandle(commandListHandle);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    std::unique_ptr<L0::ult::Module> mockModule = std::make_unique<L0::ult::Module>(device, nullptr, ModuleType::builtin);
    Mock<::L0::KernelImp> kernel;
    kernel.module = mockModule.get();

    memoryOperationsHandler->captureGfxAllocationsForMakeResident = true;

    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto allocIter = std::find(memoryOperationsHandler->gfxAllocationsForMakeResident.begin(),
                               memoryOperationsHandler->gfxAllocationsForMakeResident.end(),
                               bindlessHelper->getHeap(NEO::BindlessHeapsHelper::specialSsh)->getGraphicsAllocation());
    EXPECT_EQ(memoryOperationsHandler->gfxAllocationsForMakeResident.end(), allocIter);
    commandList->destroy();
}

HWTEST2_F(L0DebuggerGlobalStatelessTest,
          givenGlobalStatelessWhenCmdListExecutedOnQueueThenQueueDispatchesSurfaceStateOnceToGlobalStatelessHeap,
          IsAtLeastXeCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(productFamily, device, csr, &queueDesc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);

    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto cmdListHandle = commandList->toHandle();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;

    auto statelessSurfaceHeap = csr->getGlobalStatelessHeap();
    ASSERT_NE(nullptr, statelessSurfaceHeap);

    returnValue = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    commandList->close();

    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto debugSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(statelessSurfaceHeap->getCpuBase());
    ASSERT_NE(debugSurfaceState, nullptr);
    auto debugSurface = static_cast<::L0::DeviceImp *>(device)->getDebugSurface();
    ASSERT_NE(debugSurface, nullptr);
    ASSERT_EQ(debugSurface->getGpuAddress(), debugSurfaceState->getSurfaceBaseAddress());

    const auto mocsNoCache = device->getNEODevice()->getGmmHelper()->getUncachedMOCS();
    const auto actualMocs = debugSurfaceState->getMemoryObjectControlState();

    EXPECT_EQ(actualMocs, mocsNoCache);

    SurfaceStateBufferLength length;
    length.length = static_cast<uint32_t>(debugSurface->getUnderlyingBufferSize() - 1);

    EXPECT_EQ(length.surfaceState.depth + 1u, debugSurfaceState->getDepth());
    EXPECT_EQ(length.surfaceState.width + 1u, debugSurfaceState->getWidth());
    EXPECT_EQ(length.surfaceState.height + 1u, debugSurfaceState->getHeight());

    memset(debugSurfaceState, 0, sizeof(RENDER_SURFACE_STATE));

    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    char zeroBuffer[sizeof(RENDER_SURFACE_STATE)];
    memset(zeroBuffer, 0, sizeof(RENDER_SURFACE_STATE));

    auto memCmpResult = memcmp(debugSurfaceState, zeroBuffer, sizeof(RENDER_SURFACE_STATE));
    EXPECT_EQ(0, memCmpResult);

    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
