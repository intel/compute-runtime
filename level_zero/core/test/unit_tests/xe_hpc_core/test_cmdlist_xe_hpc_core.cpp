/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/memory_manager/mock_prefetch_manager.h"
#include "shared/test/common/mocks/mock_ostime.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

struct LocalMemoryModuleFixture : public ModuleFixture {
    void setUp() {
        DebugManager.flags.EnableLocalMemory.set(1);
        ModuleFixture::setUp();
    }
    DebugManagerStateRestore restore;
};

using CommandListAppendLaunchKernelXeHpcCore = Test<LocalMemoryModuleFixture>;
HWTEST2_F(CommandListAppendLaunchKernelXeHpcCore, givenKernelUsingSyncBufferWhenAppendLaunchCooperativeKernelIsCalledThenCorrectValueIsReturned, IsXeHpcCore) {
    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &hwConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::CooperativeCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.usesSyncBuffer = true;
    kernelAttributes.numGrfRequired = GrfConfig::DefaultGrfNumber;
    CmdListKernelLaunchParams launchParams = {};
    launchParams.isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(&kernel, &groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    {
        VariableBackup<EngineGroupType> engineGroupType{&pCommandList->engineGroupType};
        VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
        engineGroupType = EngineGroupType::RenderCompute;
        hwRevId = hwConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
        result = pCommandList->appendLaunchKernelWithParams(&kernel, &groupCount, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

        ze_group_count_t groupCount1{1, 1, 1};
        result = pCommandList->appendLaunchKernelWithParams(&kernel, &groupCount1, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
}

using CommandListStatePrefetchXeHpcCore = Test<ModuleFixture>;

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenUnifiedSharedMemoryWhenPrefetchApiIsCalledThenDontRequestMemoryPrefetchByDefault, IsXeHpcCore) {
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

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

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsWhenPrefetchApiIsCalledThenRequestMemoryPrefetch, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(1);

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

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

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsSetWhenPrefetchApiIsCalledOnUnifiedSharedMemoryThenAppendAllocationForPrefetch, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(1);
    DebugManager.flags.UseKmdMigration.set(1);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->prefetchManager.reset(new MockPrefetchManager());

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

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

    auto prefetchManager = static_cast<MockPrefetchManager *>(memoryManager->prefetchManager.get());
    EXPECT_EQ(1u, prefetchManager->allocations.size());

    context->freeMem(ptr);
}

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsSetWhenPrefetchApiIsCalledOnUnifiedDeviceMemoryThenDontAppendAllocationForPrefetch, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(1);
    DebugManager.flags.UseKmdMigration.set(1);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->prefetchManager.reset(new MockPrefetchManager());

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(pCommandList->isMemoryPrefetchRequested());

    auto prefetchManager = static_cast<MockPrefetchManager *>(memoryManager->prefetchManager.get());
    EXPECT_EQ(0u, prefetchManager->allocations.size());

    context->freeMem(ptr);
}

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsSetWhenPrefetchApiIsCalledOnUnifiedHostMemoryThenDontAppendAllocationForPrefetch, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(1);
    DebugManager.flags.UseKmdMigration.set(1);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->prefetchManager.reset(new MockPrefetchManager());

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(pCommandList->isMemoryPrefetchRequested());

    auto prefetchManager = static_cast<MockPrefetchManager *>(memoryManager->prefetchManager.get());
    EXPECT_EQ(0u, prefetchManager->allocations.size());

    context->freeMem(ptr);
}

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsSetWhenPrefetchApiIsCalledOnUnifiedDeviceMemoryThenDontCallSetMemPrefetchOnTheAssociatedDevice, IsXeHpcCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    DebugManagerStateRestore restore;
    DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(1);
    DebugManager.flags.UseKmdMigration.set(1);

    EXPECT_EQ(0b0001u, neoDevice->deviceBitfield.to_ulong());

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->prefetchManager.reset(new MockPrefetchManager());

    createKernel();
    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = context->allocDeviceMem(device->toHandle(), &deviceDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = commandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, event->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(memoryManager->setMemPrefetchCalled);

    context->freeMem(ptr);
    commandList->destroy();
}

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsSetWhenPrefetchApiIsCalledOnUnifiedSharedMemoryThenCallSetMemPrefetchOnTheAssociatedDevice, IsXeHpcCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    DebugManagerStateRestore restore;
    DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(1);
    DebugManager.flags.UseKmdMigration.set(1);

    neoDevice->deviceBitfield = 0b0010;

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->prefetchManager.reset(new MockPrefetchManager());

    createKernel();
    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};
    auto commandList = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::RenderCompute, returnValue);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = commandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, event->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(memoryManager->setMemPrefetchCalled);
    EXPECT_EQ(1u, memoryManager->memPrefetchSubDeviceId);

    context->freeMem(ptr);
    commandList->destroy();
}

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsSetWhenPrefetchApiIsCalledOnUnifiedSharedMemoryThenCallMigrateAllocationsToGpu, IsXeHpcCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    DebugManagerStateRestore restore;
    DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(1);
    DebugManager.flags.UseKmdMigration.set(1);

    neoDevice->deviceBitfield = 0b1000;

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->prefetchManager.reset(new MockPrefetchManager());

    createKernel();
    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};

    ze_command_list_handle_t commandListHandle = CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)->toHandle();
    auto commandList = CommandList::fromHandle(commandListHandle);
    auto commandQueue = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    result = commandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto prefetchManager = static_cast<MockPrefetchManager *>(memoryManager->prefetchManager.get());
    EXPECT_EQ(1u, prefetchManager->allocations.size());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, event->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(memoryManager->setMemPrefetchCalled);
    EXPECT_EQ(3u, memoryManager->memPrefetchSubDeviceId);

    EXPECT_TRUE(prefetchManager->migrateAllocationsToGpuCalled);
    EXPECT_EQ(0u, prefetchManager->allocations.size());

    context->freeMem(ptr);
    commandList->destroy();
    commandQueue->destroy();
}

using CommandListEventFenceTestsXeHpcCore = Test<ModuleFixture>;

HWTEST2_F(CommandListEventFenceTestsXeHpcCore, givenCommandListWithProfilingEventAfterCommandWhenRevId03ThenMiFenceIsAdded, IsXeHpcCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    auto hwInfo = commandList->commandContainer.getDevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->platform.usRevId = 0x03;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    commandList->appendEventForProfiling(event.get(), false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_MEM_FENCE *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListEventFenceTestsXeHpcCore, givenCommandListWithRegularEventAfterCommandWhenRevId03ThenMiFenceIsAdded, IsXeHpcCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    auto hwInfo = commandList->commandContainer.getDevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->platform.usRevId = 0x03;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    commandList->appendSignalEventPostWalker(event.get(), false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_MEM_FENCE *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

using CommandListAppendRangesBarrierXeHpcCore = Test<DeviceFixture>;

HWTEST2_F(CommandListAppendRangesBarrierXeHpcCore, givenCallToAppendRangesBarrierThenPipeControlProgrammed, IsXeHpcCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    uint64_t gpuAddress = 0x1200;
    void *buffer = reinterpret_cast<void *>(gpuAddress);
    size_t size = 0x1100;

    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    NEO::SvmAllocationData allocData(0);
    allocData.size = size;
    allocData.gpuAllocations.addAllocation(&mockAllocation);
    device->getDriverHandle()->getSvmAllocsManager()->insertSVMAlloc(allocData);
    const void *ranges[] = {buffer};
    const size_t sizes[] = {size};
    commandList->applyMemoryRangesBarrier(1, sizes, ranges);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto pipeControlCmd = reinterpret_cast<PIPE_CONTROL *>(*itor);
    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
    EXPECT_TRUE(pipeControlCmd->getUnTypedDataPortCacheFlush());
}

HWTEST2_F(CommandListAppendLaunchKernelXeHpcCore,
          givenHwSupportsSystemFenceWhenKernelNotUsingSystemMemoryAllocationsAndEventNotHostSignalScopeThenExpectsNoSystemFenceUsed, IsXeHpcCore) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    ze_result_t result = ZE_RESULT_SUCCESS;

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &hwConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = hwConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::Kernel> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.residencyContainer.push_back(kernelAllocation);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, &groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->commandContainer.getCommandStream()->getCpuBase(),
        commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(CommandListAppendLaunchKernelXeHpcCore,
          givenHwSupportsSystemFenceWhenKernelUsingUsmHostMemoryAllocationsAndEventNotHostSignalScopeThenExpectsNoSystemFenceUsed, IsXeHpcCore) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    ze_result_t result = ZE_RESULT_SUCCESS;

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &hwConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = hwConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::Kernel> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.residencyContainer.push_back(kernelAllocation);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, &groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->commandContainer.getCommandStream()->getCpuBase(),
        commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(CommandListAppendLaunchKernelXeHpcCore,
          givenHwSupportsSystemFenceWhenMigrationOnComputeKernelUsingUsmSharedCpuMemoryAllocationsAndEventNotHostSignalScopeThenExpectsNoSystemFenceUsed, IsXeHpcCore) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    ze_result_t result = ZE_RESULT_SUCCESS;

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &hwConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = hwConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);

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

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->appendPageFaultCopy(dstAllocation, srcAllocation, size, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isBuiltInKernel);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
    EXPECT_TRUE(commandList->usedKernelLaunchParams.isDestinationAllocationInSystemMemory);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->commandContainer.getCommandStream()->getCpuBase(),
        commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(CommandListAppendLaunchKernelXeHpcCore,
          givenHwSupportsSystemFenceWhenKernelUsingIndirectSystemMemoryAllocationsAndEventNotHostSignalScopeThenExpectsNoSystemFenceUsed, IsXeHpcCore) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    ze_result_t result = ZE_RESULT_SUCCESS;

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &hwConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = hwConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::Kernel> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.residencyContainer.push_back(kernelAllocation);

    kernel.unifiedMemoryControls.indirectHostAllocationsAllowed = true;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_DEVICE;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, &groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->commandContainer.getCommandStream()->getCpuBase(),
        commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(CommandListAppendLaunchKernelXeHpcCore,
          givenHwSupportsSystemFenceWhenKernelUsingDeviceMemoryAllocationsAndEventHostSignalScopeThenExpectsSystemFenceUsed, IsXeHpcCore) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    ze_result_t result = ZE_RESULT_SUCCESS;

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &hwConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = hwConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    result = context->allocDeviceMem(device->toHandle(),
                                     &deviceDesc,
                                     size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::Kernel> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.residencyContainer.push_back(kernelAllocation);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, &groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->commandContainer.getCommandStream()->getCpuBase(),
        commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

HWTEST2_F(CommandListAppendLaunchKernelXeHpcCore,
          givenHwSupportsSystemFenceWhenKernelUsingUsmHostMemoryAllocationsAndEventHostSignalScopeThenExpectsSystemFenceUsed, IsXeHpcCore) {
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    ze_result_t result = ZE_RESULT_SUCCESS;

    auto &hwInfo = *device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &hwConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = hwConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);

    constexpr size_t size = 4096u;
    constexpr size_t alignment = 4096u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    result = context->allocHostMem(&hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, ptr);

    Mock<::L0::Kernel> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto allocData = driverHandle->getSvmAllocsManager()->getSVMAlloc(ptr);
    ASSERT_NE(nullptr, allocData);
    auto kernelAllocation = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, kernelAllocation);
    kernel.residencyContainer.push_back(kernelAllocation);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = 0;
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    result = commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernelWithParams(&kernel, &groupCount, event.get(), launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList commands;
    ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
        commands,
        commandList->commandContainer.getCommandStream()->getCpuBase(),
        commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto walkerCmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &postSyncData = walkerCmd->getPostSync();
    EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());

    result = context->freeMem(ptr);
    ASSERT_EQ(result, ZE_RESULT_SUCCESS);
}

struct AppendMemoryLockedCopyFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();

        nonUsmHostPtr = new char[sz];
        ze_device_mem_alloc_desc_t deviceDesc = {};
        context->allocDeviceMem(device->toHandle(), &deviceDesc, sz, 1u, &devicePtr);
    }
    void tearDown() {
        delete[] nonUsmHostPtr;
        context->freeMem(devicePtr);
        DeviceFixture::tearDown();
    }

    DebugManagerStateRestore restore;
    char *nonUsmHostPtr;
    void *devicePtr;
    size_t sz = 4 * MemoryConstants::megaByte;
};

using AppendMemoryLockedCopyTest = Test<AppendMemoryLockedCopyFixture>;

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmHostPtrWhenPreferCopyThroughLockedPtrCalledThenReturnTrue, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    NEO::SvmAllocationData *srcAllocData;
    NEO::SvmAllocationData *dstAllocData;
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(nonUsmHostPtr, 1024, &srcAllocData);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, &dstAllocData);
    EXPECT_TRUE(cmdList.preferCopyThroughLockedPtr(dstAllocData, dstFound, srcAllocData, srcFound, 1024));
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenIsSuitableUSMDeviceAllocThenReturnCorrectValue, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    NEO::SvmAllocationData *srcAllocData;
    NEO::SvmAllocationData *dstAllocData;
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(nonUsmHostPtr, 1024, &srcAllocData);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, &dstAllocData);
    EXPECT_FALSE(cmdList.isSuitableUSMDeviceAlloc(srcAllocData, srcFound));
    EXPECT_TRUE(cmdList.isSuitableUSMDeviceAlloc(dstAllocData, dstFound));
}

struct LocalMemoryMultiSubDeviceFixture : public SingleRootMultiSubDeviceFixture {
    void setUp() {
        DebugManager.flags.EnableLocalMemory.set(1);
        DebugManager.flags.EnableImplicitScaling.set(1);
        SingleRootMultiSubDeviceFixture::setUp();
    }
    DebugManagerStateRestore restore;
};

using LocalMemoryMultiSubDeviceTest = Test<LocalMemoryMultiSubDeviceFixture>;

HWTEST2_F(LocalMemoryMultiSubDeviceTest, givenImmediateCommandListWhenIsSuitableUSMDeviceAllocWithColouredBufferThenReturnFalse, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);

    void *devicePtr;
    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(), &deviceDesc, 2 * MemoryConstants::megaByte, 1u, &devicePtr);

    NEO::SvmAllocationData *allocData;
    auto allocFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 2 * MemoryConstants::megaByte, &allocData);
    EXPECT_FALSE(cmdList.isSuitableUSMDeviceAlloc(allocData, allocFound));
    context->freeMem(devicePtr);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmHostPtrAndFlagDisabledWhenPreferCopyThroughLockedPtrCalledThenReturnFalse, IsXeHpcCore) {
    DebugManager.flags.ExperimentalCopyThroughLock.set(0);
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    NEO::SvmAllocationData *srcAllocData;
    NEO::SvmAllocationData *dstAllocData;
    auto srcFound = device->getDriverHandle()->findAllocationDataForRange(nonUsmHostPtr, 1024, &srcAllocData);
    auto dstFound = device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, &dstAllocData);
    EXPECT_FALSE(cmdList.preferCopyThroughLockedPtr(dstAllocData, dstFound, srcAllocData, srcFound, 1024));
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmHostPtrWhenCopyH2DThenLockPtr, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;

    NEO::SvmAllocationData *allocData;
    device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, &allocData);
    auto dstAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

    EXPECT_EQ(nullptr, dstAlloc->getLockedPtr());
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr);
    EXPECT_EQ(1u, reinterpret_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager())->lockResourceCalled);
    EXPECT_NE(nullptr, dstAlloc->getLockedPtr());
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmHostPtrWhenCopyD2HThenLockPtr, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;

    NEO::SvmAllocationData *allocData;
    device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, &allocData);
    auto dstAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

    EXPECT_EQ(nullptr, dstAlloc->getLockedPtr());
    cmdList.appendMemoryCopy(nonUsmHostPtr, devicePtr, 1024, nullptr, 0, nullptr);
    EXPECT_EQ(1u, reinterpret_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager())->lockResourceCalled);
    EXPECT_NE(nullptr, dstAlloc->getLockedPtr());
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmHostPtrWhenCopyH2DAndDstPtrLockedThenDontLockAgain, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;

    NEO::SvmAllocationData *allocData;
    device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, &allocData);
    auto dstAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());

    device->getDriverHandle()->getMemoryManager()->lockResource(dstAlloc);

    EXPECT_EQ(1u, reinterpret_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager())->lockResourceCalled);
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr);
    EXPECT_EQ(1u, reinterpret_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager())->lockResourceCalled);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmHostPtrWhenCopyH2DThenUseMemcpyAndReturnSuccess, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;

    memset(nonUsmHostPtr, 1, 1024);

    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    NEO::SvmAllocationData *allocData;
    device->getDriverHandle()->findAllocationDataForRange(devicePtr, 1024, &allocData);
    auto dstAlloc = allocData->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    auto lockedPtr = reinterpret_cast<char *>(dstAlloc->getLockedPtr());
    EXPECT_EQ(0, memcmp(lockedPtr, nonUsmHostPtr, 1024));
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndSignalEventAndNonUsmHostPtrWhenCopyH2DThenSignalEvent, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_NOT_READY);
    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, event->toHandle(), 0, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_SUCCESS);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndSignalEventAndCpuMemcpyWhenGpuHangThenDontSynchronizeEvent, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;
    reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(cmdList.csr)->callBaseWaitForCompletionWithTimeout = false;
    reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(cmdList.csr)->returnWaitForCompletionWithTimeout = WaitStatus::GpuHang;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_NOT_READY);
    cmdList.appendBarrier(nullptr, 0, nullptr);
    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, event->toHandle(), 0, nullptr);
    EXPECT_EQ(res, ZE_RESULT_ERROR_DEVICE_LOST);

    EXPECT_EQ(event->queryStatus(), ZE_RESULT_NOT_READY);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenCpuMemcpyWithoutBarrierThenDontWaitForTagUpdate, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;

    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    uint32_t waitForFlushTagUpdateCalled = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(cmdList.csr)->waitForCompletionWithTimeoutTaskCountCalled;
    EXPECT_EQ(waitForFlushTagUpdateCalled, 0u);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenCpuMemcpyWithBarrierThenWaitForTagUpdate, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;

    cmdList.appendBarrier(nullptr, 0, nullptr);
    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    uint32_t waitForFlushTagUpdateCalled = reinterpret_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(cmdList.csr)->waitForCompletionWithTimeoutTaskCountCalled;
    EXPECT_EQ(waitForFlushTagUpdateCalled, 1u);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenAppendBarrierThenSetDependenciesPresent, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;

    EXPECT_FALSE(cmdList.dependenciesPresent);

    cmdList.appendBarrier(nullptr, 0, nullptr);

    EXPECT_TRUE(cmdList.dependenciesPresent);

    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    EXPECT_FALSE(cmdList.dependenciesPresent);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListWhenAppendWaitOnEventsThenSetDependenciesPresent, IsXeHpcCore) {
    MockCommandListImmediateHw<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;

    EXPECT_FALSE(cmdList.dependenciesPresent);

    cmdList.appendWaitOnEvents(0, nullptr);

    EXPECT_TRUE(cmdList.dependenciesPresent);

    auto res = cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr);
    EXPECT_EQ(res, ZE_RESULT_SUCCESS);

    EXPECT_FALSE(cmdList.dependenciesPresent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
class MockAppendMemoryLockedCopyTestImmediateCmdList : public MockCommandListImmediateHw<gfxCoreFamily> {
  public:
    MockAppendMemoryLockedCopyTestImmediateCmdList() : MockCommandListImmediateHw<gfxCoreFamily>() {}
    ze_result_t appendMemoryCopyKernelWithGA(void *dstPtr, NEO::GraphicsAllocation *dstPtrAlloc,
                                             uint64_t dstOffset, void *srcPtr,
                                             NEO::GraphicsAllocation *srcPtrAlloc,
                                             uint64_t srcOffset, uint64_t size,
                                             uint64_t elementSize, Builtin builtin,
                                             Event *signalEvent,
                                             bool isStateless) override {
        appendMemoryCopyKernelWithGACalled++;
        return ZE_RESULT_SUCCESS;
    }
    ze_result_t appendBarrier(ze_event_handle_t hSignalEvent,
                              uint32_t numWaitEvents,
                              ze_event_handle_t *phWaitEvents) override {
        appendBarrierCalled++;
        return MockCommandListImmediateHw<gfxCoreFamily>::appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents);
    }

    uint32_t appendBarrierCalled = 0;
    uint32_t appendMemoryCopyKernelWithGACalled = 0;
};

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndUsmSrcHostPtrWhenCopyH2DThenUseGpuMemcpy, IsXeHpcCore) {
    MockAppendMemoryLockedCopyTestImmediateCmdList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *usmSrcPtr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 1024, 1u, &usmSrcPtr);

    cmdList.appendMemoryCopy(devicePtr, usmSrcPtr, 1024, nullptr, 0, nullptr);
    EXPECT_GE(cmdList.appendMemoryCopyKernelWithGACalled, 1u);
    context->freeMem(usmSrcPtr);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndUsmDstHostPtrWhenCopyThenUseGpuMemcpy, IsXeHpcCore) {
    MockAppendMemoryLockedCopyTestImmediateCmdList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *usmHostDstPtr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 1024, 1u, &usmHostDstPtr);

    cmdList.appendMemoryCopy(usmHostDstPtr, nonUsmHostPtr, 1024, nullptr, 0, nullptr);
    EXPECT_GE(cmdList.appendMemoryCopyKernelWithGACalled, 1u);
    context->freeMem(usmHostDstPtr);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndUsmSrcHostPtrWhenCopyThenUseGpuMemcpy, IsXeHpcCore) {
    MockAppendMemoryLockedCopyTestImmediateCmdList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    void *usmHostSrcPtr;
    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, 1024, 1u, &usmHostSrcPtr);

    cmdList.appendMemoryCopy(nonUsmHostPtr, usmHostSrcPtr, 1024, nullptr, 0, nullptr);
    EXPECT_GE(cmdList.appendMemoryCopyKernelWithGACalled, 1u);
    context->freeMem(usmHostSrcPtr);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmSrcHostPtrWhenSizeTooLargeThenUseGpuMemcpy, IsXeHpcCore) {
    MockAppendMemoryLockedCopyTestImmediateCmdList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 3 * MemoryConstants::megaByte, nullptr, 0, nullptr);
    EXPECT_GE(cmdList.appendMemoryCopyKernelWithGACalled, 1u);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndNonUsmDstHostPtrWhenSizeTooLargeThenUseGpuMemcpy, IsXeHpcCore) {
    MockAppendMemoryLockedCopyTestImmediateCmdList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.appendMemoryCopy(nonUsmHostPtr, devicePtr, 2 * MemoryConstants::kiloByte, nullptr, 0, nullptr);
    EXPECT_GE(cmdList.appendMemoryCopyKernelWithGACalled, 1u);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndD2HCopyWhenSizeTooLargeButFlagSetThenUseCpuMemcpy, IsXeHpcCore) {
    DebugManager.flags.ExperimentalD2HCpuCopyThreshold.set(2048);
    MockAppendMemoryLockedCopyTestImmediateCmdList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;

    cmdList.appendMemoryCopy(nonUsmHostPtr, devicePtr, 2 * MemoryConstants::kiloByte, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalled, 0u);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndH2DCopyWhenSizeTooLargeButFlagSetThenUseCpuMemcpy, IsXeHpcCore) {
    DebugManager.flags.ExperimentalH2DCpuCopyThreshold.set(3 * MemoryConstants::megaByte);
    MockAppendMemoryLockedCopyTestImmediateCmdList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 3 * MemoryConstants::megaByte, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendMemoryCopyKernelWithGACalled, 0u);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndCpuMemcpyWithDependencyThenAppendBarrierCalled, IsXeHpcCore) {
    MockAppendMemoryLockedCopyTestImmediateCmdList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    auto phEvent = event->toHandle();
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 2 * MemoryConstants::kiloByte, nullptr, 1, &phEvent);
    EXPECT_EQ(cmdList.appendBarrierCalled, 1u);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndCpuMemcpyWithoutDependencyThenAppendBarrierNotCalled, IsXeHpcCore) {
    MockAppendMemoryLockedCopyTestImmediateCmdList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 2 * MemoryConstants::kiloByte, nullptr, 0, nullptr);
    EXPECT_EQ(cmdList.appendBarrierCalled, 0u);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndTimestampFlagSetWhenCpuMemcpyThenSetCorrectGpuTimestamps, IsXeHpcCore) {
    MockAppendMemoryLockedCopyTestImmediateCmdList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;
    neoDevice->setOSTime(new NEO::MockOSTimeWithConstTimestamp());

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    auto phEvent = event->toHandle();
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 2 * MemoryConstants::kiloByte, phEvent, 0, nullptr);
    ze_kernel_timestamp_result_t resultTimestamp = {};
    auto result = event->queryKernelTimestamp(&resultTimestamp);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_EQ(resultTimestamp.context.kernelStart, NEO::MockDeviceTimeWithConstTimestamp::GPU_TIMESTAMP);
    EXPECT_EQ(resultTimestamp.global.kernelStart, NEO::MockDeviceTimeWithConstTimestamp::GPU_TIMESTAMP);
    EXPECT_EQ(resultTimestamp.context.kernelEnd, NEO::MockDeviceTimeWithConstTimestamp::GPU_TIMESTAMP);
    EXPECT_EQ(resultTimestamp.global.kernelEnd, NEO::MockDeviceTimeWithConstTimestamp::GPU_TIMESTAMP);
}

HWTEST2_F(AppendMemoryLockedCopyTest, givenImmediateCommandListAndTimestampFlagNotSetWhenCpuMemcpyThenDontSetGpuTimestamps, IsXeHpcCore) {
    MockAppendMemoryLockedCopyTestImmediateCmdList<gfxCoreFamily> cmdList;
    cmdList.initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    cmdList.csr = device->getNEODevice()->getInternalEngine().commandStreamReceiver;
    neoDevice->setOSTime(new NEO::MockOSTimeWithConstTimestamp());

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    auto phEvent = event->toHandle();
    cmdList.appendMemoryCopy(devicePtr, nonUsmHostPtr, 2 * MemoryConstants::kiloByte, phEvent, 0, nullptr);
    ze_kernel_timestamp_result_t resultTimestamp = {};
    auto result = event->queryKernelTimestamp(&resultTimestamp);
    EXPECT_EQ(result, ZE_RESULT_SUCCESS);

    EXPECT_NE(resultTimestamp.context.kernelEnd, NEO::MockDeviceTimeWithConstTimestamp::GPU_TIMESTAMP);
}

using CreateCommandListXeHpcTest = Test<DeviceFixture>;

HWTEST2_F(CreateCommandListXeHpcTest, givenXeHpcPlatformsWhenImmediateCommandListCreatedThenHeapSharingEnabledWithFlushTask, IsXeHpcCore) {
    std::unique_ptr<L0::ult::CommandList> commandListImmediate;

    auto &hwInfo = device->getHwInfo();
    auto &defaultEngine = neoDevice->getDefaultEngine();

    auto engineGroupType = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily).getEngineGroupType(defaultEngine.getEngineType(), defaultEngine.getEngineUsage(), hwInfo);

    ze_command_queue_desc_t queueDesc{};
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;

    ze_result_t returnValue;
    commandListImmediate.reset(whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, engineGroupType, returnValue)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_TRUE(commandListImmediate->isFlushTaskSubmissionEnabled);
    EXPECT_EQ(commandListImmediate->isFlushTaskSubmissionEnabled, commandListImmediate->immediateCmdListHeapSharing);
}

HWTEST2_F(CreateCommandListXeHpcTest, whenCreateImmediateCommandListThenAllocationListFilledWithCommandBuffer, IsXeHpcCore) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily,
                                                                              device,
                                                                              &desc,
                                                                              internalEngine,
                                                                              NEO::EngineGroupType::RenderCompute,
                                                                              returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_FALSE(static_cast<DeviceImp *>(device)->allocationsForReuse->peekIsEmpty());
}

HWTEST2_F(CreateCommandListXeHpcTest, whenFlagDisabledAndCreateImmediateCommandListThenAllocationListEmpty, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.SetAmountOfReusableAllocations.set(0);
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily,
                                                                              device,
                                                                              &desc,
                                                                              internalEngine,
                                                                              NEO::EngineGroupType::RenderCompute,
                                                                              returnValue));
    ASSERT_NE(nullptr, commandList);
    EXPECT_TRUE(static_cast<DeviceImp *>(device)->allocationsForReuse->peekIsEmpty());
}

} // namespace ult
} // namespace L0
