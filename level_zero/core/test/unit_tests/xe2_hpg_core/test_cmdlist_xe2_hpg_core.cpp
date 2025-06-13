/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/product_helper.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/memory_manager/mock_prefetch_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.inl"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

namespace L0 {
namespace ult {

struct LocalMemoryModuleFixture : public ModuleFixture {
    void setUp() {
        debugManager.flags.EnableLocalMemory.set(1);
        ModuleFixture::setUp();
    }
    DebugManagerStateRestore restore;
};

using CommandListAppendLaunchKernelXe2HpgCore = Test<LocalMemoryModuleFixture>;

HWTEST2_F(CommandListAppendLaunchKernelXe2HpgCore, givenAppendKernelWhenKernelNotUsingSystemMemoryAllocationsAndEventNotHostSignalScopeThenExpectsNoSystemFenceUsed, IsXe2HpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    ze_result_t result = ZE_RESULT_SUCCESS;

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.argumentsResidencyContainer.push_back(kernelAllocation);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(CommandListAppendLaunchKernelXe2HpgCore,
          givenAppendKernelWhenKernelUsingUsmHostMemoryAllocationsAndEventNotHostSignalScopeThenExpectsNoSystemFenceUsed, IsXe2HpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    ze_result_t result = ZE_RESULT_SUCCESS;

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.argumentsResidencyContainer.push_back(kernelAllocation);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(CommandListAppendLaunchKernelXe2HpgCore,
          givenAppendKernelWhenMigrationOnComputeUsingUsmSharedCpuMemoryAllocationsAndEventNotHostSignalScopeThenExpectsNoSystemFenceUsed, IsXe2HpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    ze_result_t result = ZE_RESULT_SUCCESS;

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto dstAllocation = allocData->cpuAllocation;
    ASSERT_NE(nullptr, dstAllocation);
    auto srcAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, srcAllocation);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->appendPageFaultCopy(dstAllocation, srcAllocation, size, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(CommandListAppendLaunchKernelXe2HpgCore,
          givenAppendKernelWhenKernelUsingIndirectSystemMemoryAllocationsAndEventNotHostSignalScopeThenExpectsNoSystemFenceUsed, IsXe2HpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    ze_result_t result = ZE_RESULT_SUCCESS;

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.argumentsResidencyContainer.push_back(kernelAllocation);

    kernel.unifiedMemoryControls.indirectHostAllocationsAllowed = true;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(CommandListAppendLaunchKernelXe2HpgCore,
          givenAppendKernelWhenKernelUsingDeviceMemoryAllocationsAndEventHostSignalScopeThenExpectsNoSystemFenceUsed, IsXe2HpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    ze_result_t result = ZE_RESULT_SUCCESS;

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.argumentsResidencyContainer.push_back(kernelAllocation);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->getCmdContainer().getCommandStream()->getCpuBase(),
        commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

using CommandListAppendLaunchKernelXe2HpgCoreDebugger = Test<L0DebuggerHwFixture>;

HWTEST2_F(CommandListAppendLaunchKernelXe2HpgCoreDebugger, givenDebuggingEnabledWhenKernelAppendedThenIDDDoesNotHaveMidThreadPreemptionEnabled, IsXe2HpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    ze_command_queue_desc_t queueDesc = {};
    auto queue = std::make_unique<Mock<CommandQueue>>(device, device->getNEODevice()->getDefaultEngine().commandStreamReceiver, &queueDesc);

    ze_group_count_t groupCount{128, 1, 1};
    auto immediateCmdList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    immediateCmdList->cmdListType = ::L0::CommandList::CommandListType::typeImmediate;
    immediateCmdList->cmdQImmediate = queue.get();
    auto result = immediateCmdList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto cmdStream = immediateCmdList->getCmdContainer().getCommandStream();

    auto sizeBefore = cmdStream->getUsed();
    CmdListKernelLaunchParams launchParams = {};
    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    kernel.setGroupSize(1, 1, 1);

    result = immediateCmdList->appendLaunchKernelWithParams(&kernel, groupCount, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto sizeAfter = cmdStream->getUsed();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), sizeBefore),
        sizeAfter - sizeBefore));

    auto itorWalker = find<DefaultWalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker);
    auto cmdWalker = genCmdCast<DefaultWalkerType *>(*itorWalker);

    EXPECT_EQ(0u, cmdWalker->getInterfaceDescriptor().getThreadPreemption());
}

using CmdListThreadArbitrationTestXe2HpgCore = Test<CmdListThreadArbitrationFixture>;

using ThreadArbitrationSupport = IsAnyProducts<IGFX_BMG, IGFX_LUNARLAKE>;
HWTEST2_F(CmdListThreadArbitrationTestXe2HpgCore,
          givenAppendThreadArbitrationKernelToCommandListWhenExecutingCommandListThenStateComputeModeStateIsTrackedCorrectly, ThreadArbitrationSupport) {
    testBody<FamilyType>();
}

using CmdListLargeGrfTestXe2Hpg = Test<CmdListLargeGrfFixture>;

using LargeGrfSupport = IsAnyProducts<IGFX_BMG, IGFX_LUNARLAKE>;
HWTEST2_F(CmdListLargeGrfTestXe2Hpg,
          givenAppendLargeGrfKernelToCommandListWhenExecutingCommandListThenStateComputeModeStateIsTrackedCorrectly, LargeGrfSupport) {
    testBody<FamilyType>();
}

using CommandListStatePrefetchXe2HpgCore = Test<ModuleFixture>;

HWTEST2_F(CommandListStatePrefetchXe2HpgCore, givenUnifiedSharedMemoryWhenPrefetchApiIsCalledThenRequestMemoryPrefetchByDefault, IsXe2HpgCore) {
    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->prefetchManager.reset(new MockPrefetchManager());
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    DebugManagerStateRestore restore;
    debugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(true);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(pCommandList->isMemoryPrefetchRequested());

    context->freeMem(ptr);
}

HWTEST2_F(CommandListStatePrefetchXe2HpgCore, givenUnifiedSharedMemoryWhenPrefetchApiIsCalledThenRequestMemoryPrefetchByDefaultWithNoPrefetchManager, IsXe2HpgCore) {
    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->prefetchManager.reset(new MockPrefetchManager());
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    DebugManagerStateRestore restore;
    debugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(true);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(pCommandList->isMemoryPrefetchRequested());

    context->freeMem(ptr);
}

HWTEST2_F(CommandListStatePrefetchXe2HpgCore, givenUnifiedSharedMemoryWhenPrefetchApiCalledAndDebugFlagFalseThenRequestMemoryPrefetchNotCalled, IsXe2HpgCore) {
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    DebugManagerStateRestore restore;
    debugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(false);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(pCommandList->isMemoryPrefetchRequested());

    context->freeMem(ptr);
}

HWTEST2_F(CommandListStatePrefetchXe2HpgCore, givenSharedSystemAllocationOnSupportedDeviceWhenPrefetchApiIsCalledThenRequestMemoryPrefetchCalled, IsXe2HpgCore) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);
    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->prefetchManager.reset(new MockPrefetchManager());
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();

    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);

    size_t size = 10;
    void *ptr = malloc(size);

    EXPECT_NE(nullptr, ptr);

    result = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(pCommandList->isMemoryPrefetchRequested());

    free(ptr);
}

HWTEST2_F(CommandListStatePrefetchXe2HpgCore, givenSharedSystemAllocationOnSupportedDeviceWhenPrefetchApiIsCalledThenRequestMemoryPrefetchCalledWithNoPrefetchManager, IsXe2HpgCore) {
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(1);

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();

    VariableBackup<uint64_t> sharedSystemMemCapabilities{&hwInfo.capabilityTable.sharedSystemMemCapabilities};
    sharedSystemMemCapabilities = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);

    size_t size = 10;
    void *ptr = malloc(size);

    EXPECT_NE(nullptr, ptr);

    result = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(pCommandList->isMemoryPrefetchRequested());

    free(ptr);
}

HWTEST2_F(CommandListStatePrefetchXe2HpgCore, givenSharedSystemAllocationOnUnSupportedDeviceWhenPrefetchApiIsCalledThenRequestMemoryPrefetchNotCalled, IsXe2HpgCore) {
    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->prefetchManager.reset(new MockPrefetchManager());
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    DebugManagerStateRestore restore;
    debugManager.flags.EnableSharedSystemUsmSupport.set(0);

    size_t size = 10;
    void *ptr = malloc(size);

    EXPECT_NE(nullptr, ptr);

    result = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

    EXPECT_FALSE(pCommandList->isMemoryPrefetchRequested());

    free(ptr);
}

} // namespace ult
} // namespace L0
