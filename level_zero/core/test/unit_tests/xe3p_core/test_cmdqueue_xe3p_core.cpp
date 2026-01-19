/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_assert_handler.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/test_macros/test_base.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_context.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue_handle_indirect_allocs.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "gtest/gtest.h"
#include "hw_cmds_xe3p_core.h"
#include "implicit_args.h"
#include "per_product_test_definitions.h"

namespace L0 {
namespace ult {

using CommandQueueScratchTestsXe3p = Test<ModuleFixture>;

XE3P_CORETEST_F(CommandQueueScratchTestsXe3p, givenImplicitArgsScratchWhenPatchCommandsIsCalledThenCommandsAreCorrectlyPatched) {

    ze_command_queue_desc_t desc = {};
    NEO::CommandStreamReceiver *csr = nullptr;
    device->getCsrForOrdinalAndIndex(&csr, 0u, 0u, ZE_COMMAND_QUEUE_PRIORITY_NORMAL, 0, false);
    auto commandQueue = std::make_unique<MockCommandQueueHw<FamilyType::gfxCoreFamily>>(device, csr, &desc);
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandQueue->heaplessModeEnabled = true;

    NEO::ImplicitArgs implicitArgs{};
    implicitArgs.initializeHeader(1);
    auto scratchOffset = implicitArgs.getScratchPtrOffset();
    EXPECT_TRUE(scratchOffset.has_value());

    uint64_t expectedScratchPtr = 0x1234u;

    PatchComputeWalkerImplicitArgsScratch patch{};
    patch.pDestination = &implicitArgs;
    patch.offset = scratchOffset.value();
    patch.patchSize = 8u;
    patch.baseAddress = expectedScratchPtr;

    commandList->commandsToPatch.push_back(patch);

    commandQueue->patchCommands(*commandList, 0, true, nullptr);

    EXPECT_EQ(expectedScratchPtr, implicitArgs.v1.scratchPtr);

    uint64_t notExpectedScratchPtr = 0x4321u;
    size_t patchCmdIndex = commandList->commandsToPatch.size() - 1;

    auto *patchToModify = std::get_if<PatchComputeWalkerImplicitArgsScratch>(&commandList->commandsToPatch[patchCmdIndex]);
    ASSERT_NE(nullptr, patchToModify);
    patchToModify->baseAddress = notExpectedScratchPtr;

    commandQueue->patchCommands(*commandList, 0, false, nullptr);

    EXPECT_NE(notExpectedScratchPtr, implicitArgs.v1.scratchPtr);
    EXPECT_EQ(expectedScratchPtr, implicitArgs.v1.scratchPtr);
    commandList->commandsToPatch.clear();
}

using CommandQueueIndirectAllocationsXe3p = Test<ModuleFixture>;

XE3P_CORETEST_F(CommandQueueIndirectAllocationsXe3p, givenCtxWithIndirectAccessAndHeaplessStateInitWhenExecutingCommandListImmediateWithFlushTaskThenHandleIndirectAccessCalled) {

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto commandQueue = new MockCommandQueueHandleIndirectAllocs<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto cmdListHandle = commandList->toHandle();
    commandList->close();

    auto ctx = CommandListExecutionContext{&cmdListHandle,
                                           1,
                                           csr->getPreemptionMode(),
                                           device,
                                           csr->getScratchSpaceController(),
                                           csr->getGlobalStatelessHeapAllocation(),
                                           false,
                                           csr->isProgramActivePartitionConfigRequired(),
                                           false,
                                           false};

    ctx.hasIndirectAccess = true;
    ctx.isDispatchTaskCountPostSyncRequired = false;
    ctx.pipelineCmdsDispatch = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandListsRegularHeapless(ctx, 1, &cmdListHandle, nullptr, nullptr));
    EXPECT_EQ(commandQueue->handleIndirectAllocationResidencyCalledTimes, 1u);
    commandQueue->destroy();
}

XE3P_CORETEST_F(CommandQueueIndirectAllocationsXe3p, givenCtxWithNoIndirectAccessAndHeaplessStateInitWhenExecutingCommandListImmediateWithFlushTaskThenHandleIndirectAccessNotCalled) {

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto commandQueue = new MockCommandQueueHandleIndirectAllocs<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandList->close();
    auto cmdListHandle = commandList.get()->toHandle();

    auto ctx = CommandListExecutionContext{&cmdListHandle,
                                           1,
                                           csr->getPreemptionMode(),
                                           device,
                                           csr->getScratchSpaceController(),
                                           csr->getGlobalStatelessHeapAllocation(),
                                           false,
                                           csr->isProgramActivePartitionConfigRequired(),
                                           false,
                                           false};

    ctx.hasIndirectAccess = false;
    ctx.isDispatchTaskCountPostSyncRequired = false;
    ctx.pipelineCmdsDispatch = false;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandListsRegularHeapless(ctx, 1, &cmdListHandle, nullptr, nullptr));
    commandQueue->destroy();
}

using CommandQueueCacheFlushTestsXe3p = Test<ModuleFixture>;

XE3P_CORETEST_F(CommandQueueCacheFlushTestsXe3p, givenInstructionCacheWhenExecuteCommandListsRegularHeaplessThenPipeControlContainsInstructionCacheFlush) {

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandList->close();
    auto cmdListHandle = commandList.get()->toHandle();

    auto ctx = CommandListExecutionContext{&cmdListHandle,
                                           1,
                                           csr->getPreemptionMode(),
                                           device,
                                           csr->getScratchSpaceController(),
                                           csr->getGlobalStatelessHeapAllocation(),
                                           false,
                                           csr->isProgramActivePartitionConfigRequired(),
                                           false,
                                           false};

    ctx.hasIndirectAccess = false;
    ctx.isDispatchTaskCountPostSyncRequired = false;
    ctx.pipelineCmdsDispatch = false;
    csr->registerInstructionCacheFlush();

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandListsRegularHeapless(ctx, 1, &cmdListHandle, nullptr, nullptr));

    GenCmdList cmdList;
    FamilyType::Parse::parseCommandBuffer(cmdList, commandQueue->commandStream.getCpuBase(), commandQueue->commandStream.getUsed());

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(0u, pipeControls.size());

    bool instructionCacheFlushFound = false;

    for (auto &cmd : pipeControls) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*cmd);

        if (pipeControl->getInstructionCacheInvalidateEnable() == true) {
            instructionCacheFlushFound = true;
        }
    }

    EXPECT_TRUE(instructionCacheFlushFound);
    commandQueue->destroy();
}

XE3P_CORETEST_F(CommandQueueCacheFlushTestsXe3p, givenStateCacheWhenExecuteCommandListsRegularHeaplessThenPipeControlContainsStateCacheFlush) {

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto contextId = csr->getOsContext().getContextId();
    auto bindlessHeapHelper = new MockBindlesHeapsHelper(neoDevice, false);
    bindlessHeapHelper->stateCacheDirtyForContext.at(contextId) = true;
    ASSERT_TRUE(bindlessHeapHelper->getStateDirtyForContext(contextId));

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapHelper);

    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandList->close();
    auto cmdListHandle = commandList.get()->toHandle();

    auto ctx = CommandListExecutionContext{&cmdListHandle,
                                           1,
                                           csr->getPreemptionMode(),
                                           device,
                                           csr->getScratchSpaceController(),
                                           csr->getGlobalStatelessHeapAllocation(),
                                           false,
                                           csr->isProgramActivePartitionConfigRequired(),
                                           false,
                                           false};

    ctx.hasIndirectAccess = false;
    ctx.isDispatchTaskCountPostSyncRequired = false;
    ctx.pipelineCmdsDispatch = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandListsRegularHeapless(ctx, 1, &cmdListHandle, nullptr, nullptr));

    GenCmdList cmdList;
    FamilyType::Parse::parseCommandBuffer(cmdList, commandQueue->commandStream.getCpuBase(), commandQueue->commandStream.getUsed());

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(0u, pipeControls.size());

    bool stateCacheFlushFound = false;

    for (auto &cmd : pipeControls) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*cmd);

        if (pipeControl->getStateCacheInvalidationEnable() == true) {
            stateCacheFlushFound = true;
        }
    }

    EXPECT_TRUE(stateCacheFlushFound);
    EXPECT_FALSE(bindlessHeapHelper->getStateDirtyForContext(contextId));
    commandQueue->destroy();
}

XE3P_CORETEST_F(CommandQueueCacheFlushTestsXe3p, givenBindlessHelperAndStateNotDirtyWhenExecuteCommandListsRegularHeaplessThenPipeControlDoesntContainStateCacheFlush) {

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    ze_command_queue_desc_t desc = {};
    auto csr = neoDevice->getDefaultEngine().commandStreamReceiver;
    auto contextId = csr->getOsContext().getContextId();
    auto bindlessHeapHelper = new MockBindlesHeapsHelper(neoDevice, false);
    bindlessHeapHelper->clearStateDirtyForContext(contextId);
    ASSERT_FALSE(bindlessHeapHelper->getStateDirtyForContext(contextId));

    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapHelper);

    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandList->close();
    auto cmdListHandle = commandList.get()->toHandle();

    auto ctx = CommandListExecutionContext{&cmdListHandle,
                                           1,
                                           csr->getPreemptionMode(),
                                           device,
                                           csr->getScratchSpaceController(),
                                           csr->getGlobalStatelessHeapAllocation(),
                                           false,
                                           csr->isProgramActivePartitionConfigRequired(),
                                           false,
                                           false};

    ctx.hasIndirectAccess = false;
    ctx.isDispatchTaskCountPostSyncRequired = false;
    ctx.pipelineCmdsDispatch = false;

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandListsRegularHeapless(ctx, 1, &cmdListHandle, nullptr, nullptr));

    GenCmdList cmdList;
    FamilyType::Parse::parseCommandBuffer(cmdList, commandQueue->commandStream.getCpuBase(), commandQueue->commandStream.getUsed());

    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(0u, pipeControls.size());

    bool stateCacheFlushFound = false;

    for (auto &cmd : pipeControls) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*cmd);

        if (pipeControl->getStateCacheInvalidationEnable() == true) {
            stateCacheFlushFound = true;
        }
    }

    EXPECT_FALSE(stateCacheFlushFound);
    EXPECT_FALSE(bindlessHeapHelper->getStateDirtyForContext(contextId));
    commandQueue->destroy();
}
using CommandQueueHeaplessXe3p = Test<ModuleFixture>;

XE3P_CORETEST_F(CommandQueueHeaplessXe3p, givenSecondaryContextQueueWhenExecutingThenSetPartitionOffsetRegisters) {

    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);
    debugManager.flags.ContextGroupSize.set(8);
    debugManager.flags.EnableImplicitScaling.set(1);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrRcsNode = false;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.featureTable.ftrBcsInfo = 0;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    MockExecutionEnvironment executionEnvironment(&hwInfo, false, 1);
    executionEnvironment.incRefInternal();

    UltDeviceFactory deviceFactory{1, 2, executionEnvironment};

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(deviceFactory.rootDevices[0]));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandle>>();
    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    ze_command_queue_desc_t desc = {};
    auto csr1 = deviceFactory.rootDevices[0]->getSecondaryEngineCsr({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}, std::nullopt, false)->commandStreamReceiver;
    auto csr2 = deviceFactory.rootDevices[0]->getSecondaryEngineCsr({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}, std::nullopt, false)->commandStreamReceiver;

    EXPECT_NE(csr1, csr2);

    auto commandQueue2 = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr2, &desc);
    commandQueue2->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandList->close();
    auto cmdListHandle = commandList.get()->toHandle();

    commandQueue2->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandQueue2->commandStream);
    auto lrmCmds = findAll<MI_LOAD_REGISTER_MEM *>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end());
    bool found = false;

    for (auto &lrmCmd : lrmCmds) {
        auto lrm = genCmdCast<MI_LOAD_REGISTER_MEM *>(*lrmCmd);
        if (lrm->getRegisterAddress() == PartitionRegisters<FamilyType>::wparidCCSOffset) {
            found = true;
            break;
        }
    }

    EXPECT_TRUE(found);

    commandQueue2->destroy();
}

using CommandQueueWithAssertXe3p = Test<DeviceFixture>;

XE3P_CORETEST_F(CommandQueueWithAssertXe3p, givenCmdListWithAssertAndStateHeaplessInitWhenExecutingThenCommandQueuesPropertyIsSet) {

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    ze_command_queue_desc_t desc = {};

    auto assertHandler = new MockAssertHandler(device->getNEODevice());
    device->getNEODevice()->getRootDeviceEnvironmentRef().assertHandler.reset(assertHandler);

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

    Mock<Module> module(device, nullptr, ModuleType::user);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(NEO::defaultHwInfo->platform.eProductFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ze_group_count_t groupCount{1, 1, 1};

    kernel.descriptor.kernelAttributes.flags.usesAssert = true;

    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    commandList->close();

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    returnValue = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, false, nullptr, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_TRUE(commandQueue->cmdListWithAssertExecuted);

    commandQueue->destroy();
}

using CommandQueueWithXe3p = Test<DeviceFixture>;

XE3P_CORETEST_F(CommandQueueWithXe3p, givenHeaplessStateInitAndNonDefaultCsrWhenExecutingCmdListsForTheFirstTimeThenHeaplessPrologIsSent) {

    using STATE_SYSTEM_MEM_FENCE_ADDRESS = typename FamilyType::STATE_SYSTEM_MEM_FENCE_ADDRESS;

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    ze_command_queue_desc_t desc = {};

    auto csr = neoDevice->tryGetEngine(aub_stream::EngineType::ENGINE_CCS, EngineUsage::lowPriority)->commandStreamReceiver;

    auto commandQueue = new MockCommandQueueHw<FamilyType::gfxCoreFamily>(device, csr, &desc);
    commandQueue->initialize(false, false, false);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandList->close();
    auto cmdListHandle = commandList.get()->toHandle();

    auto ctx = CommandListExecutionContext{&cmdListHandle,
                                           1,
                                           csr->getPreemptionMode(),
                                           device,
                                           csr->getScratchSpaceController(),
                                           csr->getGlobalStatelessHeapAllocation(),
                                           false,
                                           csr->isProgramActivePartitionConfigRequired(),
                                           false,
                                           false};
    ctx.hasIndirectAccess = false;
    ctx.isDispatchTaskCountPostSyncRequired = false;
    ctx.pipelineCmdsDispatch = false;
    EXPECT_FALSE(reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr)->heaplessStateInitialized);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandListsRegularHeapless(ctx, 1, &cmdListHandle, nullptr, nullptr));

    if (!device->getHwInfo().capabilityTable.isIntegratedDevice) {
        GenCmdList cmdList, cmdList2;
        FamilyType::Parse::parseCommandBuffer(cmdList, csr->getCS().getCpuBase(), csr->getCS().getUsed());
        auto memFence = findAll<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(cmdList.begin(), cmdList.end());
        EXPECT_EQ(1u, memFence.size());
        EXPECT_TRUE(reinterpret_cast<UltCommandStreamReceiver<FamilyType> *>(csr)->heaplessStateInitialized);

        EXPECT_EQ(ZE_RESULT_SUCCESS, commandQueue->executeCommandListsRegularHeapless(ctx, 1, &cmdListHandle, nullptr, nullptr));

        FamilyType::Parse::parseCommandBuffer(cmdList2, csr->getCS().getCpuBase(), csr->getCS().getUsed());
        memFence = findAll<STATE_SYSTEM_MEM_FENCE_ADDRESS *>(cmdList2.begin(), cmdList2.end());
        EXPECT_EQ(1u, memFence.size());
    }

    commandQueue->destroy();
}

using CommandListExecuteImmediateXe3p = Test<DeviceFixture>;

XE3P_CORETEST_F(CommandListExecuteImmediateXe3p, givenHeaplessStateInitWhenExecutingCommandListImmediateWithFlushTaskThenSuccessIsReturned) {

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::kernel, false, false, nullptr, nullptr));
}

XE3P_CORETEST_F(CommandListExecuteImmediateXe3p, givenHeaplessStateInitWhenExecutingCommandListImmediateNonKernelOperationWithFlushTaskThenSuccessIsReturned) {

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::nonKernel, false, false, nullptr, nullptr));
}

XE3P_CORETEST_F(CommandListExecuteImmediateXe3p, givenHeaplessStateInitAndRegisterInstructionCacheFlushWhenExecutingCommandListImmediateWithFlushTaskThenSuccessIsReturned) {

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    ze_command_queue_desc_t desc = {};

    std::unique_ptr<L0::CommandList> commandList;
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);
    commandListImmediate.getCsr(false)->registerInstructionCacheFlush();

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::kernel, false, false, nullptr, nullptr));

    GenCmdList cmdList;
    auto &commandStream = commandListImmediate.getCsr(false)->getCS();
    FamilyType::Parse::parseCommandBuffer(cmdList, commandStream.getCpuBase(), commandStream.getUsed());
    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(0u, pipeControls.size());
    bool instructionCacheFlushFound = false;

    for (auto &cmd : pipeControls) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*cmd);

        if (pipeControl->getInstructionCacheInvalidateEnable() == true) {
            instructionCacheFlushFound = true;
        }
    }

    EXPECT_TRUE(instructionCacheFlushFound);
    EXPECT_FALSE(commandListImmediate.getCsr(false)->isInstructionCacheFlushRequired());
}

XE3P_CORETEST_F(CommandListExecuteImmediateXe3p, givenHeaplessStateInitAndStateCacheDirtyWhenExecutingCommandListImmediateWithFlushTaskThenSuccessIsReturned) {

    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);

    auto csr = commandListImmediate.getCsr(false);
    auto contextId = csr->getOsContext().getContextId();
    auto bindlessHeapHelper = new MockBindlesHeapsHelper(neoDevice, false);
    bindlessHeapHelper->stateCacheDirtyForContext.at(contextId) = true;
    ASSERT_TRUE(bindlessHeapHelper->getStateDirtyForContext(contextId));
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.reset(bindlessHeapHelper);

    EXPECT_EQ(ZE_RESULT_SUCCESS, commandListImmediate.executeCommandListImmediateWithFlushTask(false, false, false, NEO::AppendOperations::kernel, false, false, nullptr, nullptr));

    GenCmdList cmdList;
    auto &commandStream = commandListImmediate.getCsr(false)->getCS();
    FamilyType::Parse::parseCommandBuffer(cmdList, commandStream.getCpuBase(), commandStream.getUsed());
    auto pipeControls = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(0u, pipeControls.size());
    bool stateCacheFlushFound = false;

    for (auto &cmd : pipeControls) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*cmd);

        if (pipeControl->getStateCacheInvalidationEnable() == true) {
            stateCacheFlushFound = true;
        }
    }

    EXPECT_TRUE(stateCacheFlushFound);
    EXPECT_FALSE(bindlessHeapHelper->getStateDirtyForContext(contextId));
}

XE3P_CORETEST_F(CommandListExecuteImmediateXe3p,
                givenStreamStatesUnsupportedAndDirtyStatesWhenHandleHeapsAndResidencyForImmediateRegularTaskIsCalledThenStatesHaveNotChanged) {

    std::unique_ptr<L0::CommandList> commandList;
    const ze_command_queue_desc_t desc = {};

    ze_result_t returnValue;
    constexpr bool streamStatesSupported = false;
    {
        void *sshCpuBaseAddress = nullptr;
        commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
        auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);
        commandListImmediate.cmdListHeapAddressModel = HeapAddressModel::globalStateless;
        commandListImmediate.getCsr(false)->createGlobalStatelessHeap();

        commandListImmediate.requiredStreamState.stateBaseAddress.surfaceStateBaseAddress.isDirty = true;
        commandListImmediate.template handleHeapsAndResidencyForImmediateRegularTask<streamStatesSupported>(sshCpuBaseAddress);
        EXPECT_TRUE(commandListImmediate.requiredStreamState.stateBaseAddress.surfaceStateBaseAddress.isDirty);
    }
    {

        void *sshCpuBaseAddress = nullptr;
        commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
        auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);
        commandListImmediate.cmdListHeapAddressModel = HeapAddressModel::globalBindless;
        commandListImmediate.immediateCmdListHeapSharing = true;
        commandListImmediate.dynamicHeapRequired = true;
        auto &commandContainer = commandListImmediate.commandContainer;
        NEO::MockGraphicsAllocation mockHeapAllocation;
        commandContainer.getSurfaceStateHeapReserve().indirectHeapReservation->replaceGraphicsAllocation(&mockHeapAllocation);
        commandContainer.getDynamicStateHeapReserve().indirectHeapReservation->replaceGraphicsAllocation(&mockHeapAllocation);
        commandListImmediate.requiredStreamState.stateBaseAddress.bindingTablePoolBaseAddress.isDirty = true;
        commandListImmediate.requiredStreamState.stateBaseAddress.dynamicStateBaseAddress.isDirty = true;
        commandListImmediate.template handleHeapsAndResidencyForImmediateRegularTask<streamStatesSupported>(sshCpuBaseAddress);
        EXPECT_TRUE(commandListImmediate.requiredStreamState.stateBaseAddress.bindingTablePoolBaseAddress.isDirty);
        EXPECT_TRUE(commandListImmediate.requiredStreamState.stateBaseAddress.dynamicStateBaseAddress.isDirty);
    }
    {

        void *sshCpuBaseAddress = nullptr;
        commandList.reset(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
        auto &commandListImmediate = static_cast<MockCommandListImmediate<FamilyType::gfxCoreFamily> &>(*commandList);
        commandListImmediate.cmdListHeapAddressModel = HeapAddressModel::privateHeaps;
        commandListImmediate.immediateCmdListHeapSharing = false;
        commandListImmediate.dynamicHeapRequired = true;
        auto &commandContainer = commandListImmediate.commandContainer;

        NEO::MockGraphicsAllocation mockSshAllocation;
        NEO::MockGraphicsAllocation mockDshAllocation;
        NEO::ReservedIndirectHeap reservedSsh(&mockSshAllocation);
        NEO::ReservedIndirectHeap reservedDsh(&mockDshAllocation);
        auto sshHeapPtr = &reservedSsh;
        auto dshHeapPtr = &reservedDsh;
        constexpr size_t alignment = 64;
        HeapReserveArguments sshReserveArgs = {sshHeapPtr, 0, alignment};
        HeapReserveArguments dshReserveArgs = {dshHeapPtr, 0, alignment};
        class MockCommandContainer : public CommandContainer {
          public:
            using CommandContainer::reserveSpaceForDispatch;
        };
        bool dshSupport = true;
        static_cast<MockCommandContainer *>(&commandContainer)->reserveSpaceForDispatch(sshReserveArgs, dshReserveArgs, dshSupport);

        commandListImmediate.requiredStreamState.stateBaseAddress.bindingTablePoolBaseAddress.isDirty = true;
        commandListImmediate.requiredStreamState.stateBaseAddress.dynamicStateBaseAddress.isDirty = true;
        commandListImmediate.template handleHeapsAndResidencyForImmediateRegularTask<streamStatesSupported>(sshCpuBaseAddress);
        EXPECT_TRUE(commandListImmediate.requiredStreamState.stateBaseAddress.bindingTablePoolBaseAddress.isDirty);
        EXPECT_TRUE(commandListImmediate.requiredStreamState.stateBaseAddress.dynamicStateBaseAddress.isDirty);
    }
}

XE3P_CORETEST_F(CommandListExecuteImmediateXe3p, givenImmediateCmdListAndAppendingRegularCommandlistWithWaitOnEventsAndSignalEventThenUseSemaphoreAndPipeControl) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPoolHostVisible = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto eventHostVisible = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPoolHostVisible.get(), &eventDesc, device, result));

    auto waitEventPool = std::unique_ptr<L0::EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto waitEvent = std::unique_ptr<L0::Event>(Event::create<typename FamilyType::TimestampPacketType>(waitEventPool.get(), &eventDesc, device, result));

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> immCommandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, immCommandList);

    ze_event_handle_t hSignalEventHandle = eventHostVisible->toHandle();
    ze_event_handle_t hWaitEventHandle = waitEvent->toHandle();
    std::unique_ptr<L0::CommandList> commandListRegular(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandListRegular->close();
    auto commandListHandle = commandListRegular->toHandle();
    auto usedSpaceBefore = immCommandList->getCmdContainer().getCommandStream()->getUsed();
    result = immCommandList->appendCommandLists(1u, &commandListHandle, hSignalEventHandle, 1u, &hWaitEventHandle);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = immCommandList->getCmdContainer().getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      immCommandList->getCmdContainer().getCommandStream()->getCpuBase(),
                                                      usedSpaceAfter));

    auto itorSemaphore = find<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorSemaphore);

    auto itorBBStart = find<MI_BATCH_BUFFER_START *>(itorSemaphore, cmdList.end());
    ASSERT_NE(cmdList.end(), itorBBStart);

    auto itorPC = findAll<PIPE_CONTROL *>(itorBBStart, cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    bool postSyncFound = false;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_NE(cmd->getImmediateData(), Event::STATE_CLEARED);
            EXPECT_TRUE(cmd->getCommandStreamerStallEnable());
            postSyncFound = true;
        }
    }
    ASSERT_TRUE(postSyncFound);
}

XE3P_CORETEST_F(CommandListExecuteImmediateXe3p, givenImmediateCmdListAndAppendingRegularCommandlistThenCsrMakeNonTesidentSkippedFromCmdQueue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.Enable64BitAddressing.set(1);
    debugManager.flags.Enable64bAddressingStateInit.set(1);
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    ze_result_t returnValue;

    using cmdListImmediateHwType = typename L0::CommandListCoreFamilyImmediate<FamilyType::gfxCoreFamily>;

    std::unique_ptr<cmdListImmediateHwType> commandList0(static_cast<cmdListImmediateHwType *>(CommandList::createImmediate(productFamily,
                                                                                                                            device,
                                                                                                                            &desc,
                                                                                                                            false,
                                                                                                                            NEO::EngineGroupType::compute,
                                                                                                                            returnValue)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList0);

    auto &commandStreamReceiver = neoDevice->getUltCommandStreamReceiver<FamilyType>();

    std::unique_ptr<L0::CommandList> commandListRegular(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    commandListRegular->close();
    auto commandListHandle = commandListRegular->toHandle();

    ze_result_t result = ZE_RESULT_SUCCESS;
    result = commandList0->appendCommandLists(1u, &commandListHandle, nullptr, 0u, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(2u, commandStreamReceiver.makeSurfacePackNonResidentCalled);
}

} // namespace ult
} // namespace L0
