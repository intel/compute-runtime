/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/memory_manager/mock_prefetch_manager.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
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
    auto &productHelper = device->getProductHelper();
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
        hwRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);
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

    EXPECT_EQ(1u, pCommandList->getPrefetchContext().allocations.size());

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

    EXPECT_EQ(0u, pCommandList->getPrefetchContext().allocations.size());

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

    EXPECT_EQ(0u, pCommandList->getPrefetchContext().allocations.size());

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
    EXPECT_EQ(1u, memoryManager->memPrefetchSubDeviceIds[0]);

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
    EXPECT_EQ(1u, commandList->getPrefetchContext().allocations.size());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, event->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(memoryManager->setMemPrefetchCalled);
    EXPECT_EQ(3u, memoryManager->memPrefetchSubDeviceIds[0]);

    EXPECT_TRUE(prefetchManager->migrateAllocationsToGpuCalled);
    EXPECT_EQ(1u, commandList->getPrefetchContext().allocations.size());

    commandList->reset();
    EXPECT_TRUE(prefetchManager->removeAllocationsCalled);
    EXPECT_EQ(0u, commandList->getPrefetchContext().allocations.size());

    context->freeMem(ptr);
    commandList->destroy();
    commandQueue->destroy();
}

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsSetWhenPrefetchApiIsCalledForUnifiedSharedMemoryOnCmdListCopyOnlyThenCallMigrateAllocationsToGpu, IsXeHpcCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    DebugManagerStateRestore restore;
    DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(1);
    DebugManager.flags.UseKmdMigration.set(1);

    neoDevice->deviceBitfield = 0b001;

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->prefetchManager.reset(new MockPrefetchManager());

    createKernel();
    ze_result_t returnValue;
    ze_command_queue_desc_t queueDesc = {};

    ze_command_list_handle_t commandListHandle = CommandList::createImmediate(productFamily, device, &queueDesc, false, NEO::EngineGroupType::Copy, returnValue)->toHandle();
    auto commandList = CommandList::fromHandle(commandListHandle);
    auto commandQueue = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, commandList->isCopyOnly(), false, returnValue);

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
    void *srcPtr = nullptr;
    void *dstPtr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};

    auto result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &srcPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, srcPtr);

    result = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &dstPtr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, dstPtr);

    result = commandList->appendMemoryPrefetch(srcPtr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandList->appendMemoryPrefetch(dstPtr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto prefetchManager = static_cast<MockPrefetchManager *>(memoryManager->prefetchManager.get());
    EXPECT_EQ(2u, commandList->getPrefetchContext().allocations.size());

    result = commandList->appendMemoryCopy(dstPtr, srcPtr, size, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    result = commandQueue->executeCommandLists(1, &commandListHandle, nullptr, true);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(memoryManager->setMemPrefetchCalled);
    EXPECT_EQ(0u, memoryManager->memPrefetchSubDeviceIds[0]);

    EXPECT_TRUE(prefetchManager->migrateAllocationsToGpuCalled);
    EXPECT_EQ(2u, commandList->getPrefetchContext().allocations.size());

    commandList->reset();
    EXPECT_TRUE(prefetchManager->removeAllocationsCalled);
    EXPECT_EQ(0u, commandList->getPrefetchContext().allocations.size());

    context->freeMem(srcPtr);
    context->freeMem(dstPtr);

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
    commandList->appendEventForProfiling(event.get(), false);

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
    commandList->appendSignalEventPostWalker(event.get());

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
    auto &productHelper = device->getProductHelper();

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

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
    auto &productHelper = device->getProductHelper();

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

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
    auto &productHelper = device->getProductHelper();

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

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
    EXPECT_FALSE(commandList->usedKernelLaunchParams.isKernelSplitOperation);
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
    auto &productHelper = device->getProductHelper();

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

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
    auto &productHelper = device->getProductHelper();

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

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
    auto &productHelper = device->getProductHelper();

    VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
    hwRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

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

using CreateCommandListXeHpcTest = Test<DeviceFixture>;

HWTEST2_F(CreateCommandListXeHpcTest, givenXeHpcPlatformsWhenImmediateCommandListCreatedThenHeapSharingEnabledWithFlushTask, IsXeHpcCore) {
    std::unique_ptr<L0::ult::CommandList> commandListImmediate;

    auto &hwInfo = device->getHwInfo();
    auto &defaultEngine = neoDevice->getDefaultEngine();

    auto &gfxCoreHelper = neoDevice->getGfxCoreHelper();
    auto engineGroupType = gfxCoreHelper.getEngineGroupType(defaultEngine.getEngineType(), defaultEngine.getEngineUsage(), hwInfo);

    ze_command_queue_desc_t queueDesc{};
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;

    ze_result_t returnValue;
    commandListImmediate.reset(whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, engineGroupType, returnValue)));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_TRUE(commandListImmediate->isFlushTaskSubmissionEnabled);
    EXPECT_EQ(commandListImmediate->isFlushTaskSubmissionEnabled, commandListImmediate->immediateCmdListHeapSharing);
}

HWTEST2_F(CreateCommandListXeHpcTest, whenDestroyImmediateCommandListThenGlobalAllocationListFilledWithCommandBuffer, IsXeHpcCore) {
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
    commandList.reset();
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

struct AppendKernelXeHpcTestInput {
    DriverHandle *driver = nullptr;
    L0::Context *context = nullptr;
    L0::Device *device = nullptr;
};

template <int32_t usePipeControlMultiPacketEventSync>
struct CommandListAppendLaunchMultiKernelEventFixture : public LocalMemoryModuleFixture {
    void setUp() {
        DebugManager.flags.UsePipeControlMultiKernelEventSync.set(usePipeControlMultiPacketEventSync);
        LocalMemoryModuleFixture::setUp();

        input.driver = driverHandle.get();
        input.device = device;
        input.context = context;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testHostSignalScopeDeviceMemoryAppendMultiKernelCopy(AppendKernelXeHpcTestInput &input) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

        ze_result_t result = ZE_RESULT_SUCCESS;

        auto &hwInfo = *input.device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = input.device->getProductHelper();

        VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
        hwRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

        constexpr size_t size = 4096u;
        constexpr size_t alignment = 4096u;
        void *ptr = nullptr;
        const void *srcPtr = reinterpret_cast<void *>(0x1234);

        ze_device_mem_alloc_desc_t deviceDesc = {};
        result = input.context->allocDeviceMem(input.device->toHandle(),
                                               &deviceDesc,
                                               size, alignment, &ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptr);

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
        eventDesc.wait = 0;
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        result = commandList->initialize(input.device, NEO::EngineGroupType::Compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        constexpr size_t offset = 32;
        void *copyPtr = reinterpret_cast<uint8_t *>(ptr) + offset;
        result = commandList->appendMemoryCopy(copyPtr, srcPtr, size - offset, event.get(), 0, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
            commands,
            commandList->commandContainer.getCommandStream()->getCpuBase(),
            commandList->commandContainer.getCommandStream()->getUsed()));

        auto itorWalkers = findAll<WALKER_TYPE *>(commands.begin(), commands.end());
        EXPECT_NE(0u, itorWalkers.size());
        for (const auto &it : itorWalkers) {
            auto walkerCmd = genCmdCast<WALKER_TYPE *>(*it);
            auto &postSyncData = walkerCmd->getPostSync();
            EXPECT_FALSE(postSyncData.getSystemMemoryFenceRequest());
        }

        result = input.context->freeMem(ptr);
        ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testHostSignalScopeHostMemoryAppendMultiKernelCopy(AppendKernelXeHpcTestInput &input) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

        ze_result_t result = ZE_RESULT_SUCCESS;

        auto &hwInfo = *input.device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
        auto &productHelper = input.device->getProductHelper();

        VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
        hwRevId = productHelper.getHwRevIdFromStepping(REVISION_B, hwInfo);

        constexpr size_t size = 4096u;
        constexpr size_t alignment = 4096u;
        void *ptr = nullptr;
        const void *srcPtr = reinterpret_cast<void *>(0x1234);

        ze_host_mem_alloc_desc_t hostDesc = {};
        result = input.context->allocHostMem(&hostDesc, size, alignment, &ptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_NE(nullptr, ptr);

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
        eventDesc.wait = 0;
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, input.device));

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        result = commandList->initialize(input.device, NEO::EngineGroupType::Compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        constexpr size_t offset = 32;
        void *copyPtr = reinterpret_cast<uint8_t *>(ptr) + offset;
        result = commandList->appendMemoryCopy(copyPtr, srcPtr, size - offset, event.get(), 0, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList commands;
        ASSERT_TRUE(CmdParse<FamilyType>::parseCommandBuffer(
            commands,
            commandList->commandContainer.getCommandStream()->getCpuBase(),
            commandList->commandContainer.getCommandStream()->getUsed()));

        auto itorWalkers = findAll<WALKER_TYPE *>(commands.begin(), commands.end());
        EXPECT_NE(0u, itorWalkers.size());
        for (const auto &it : itorWalkers) {
            auto walkerCmd = genCmdCast<WALKER_TYPE *>(*it);
            auto &postSyncData = walkerCmd->getPostSync();
            EXPECT_TRUE(postSyncData.getSystemMemoryFenceRequest());
        }

        result = input.context->freeMem(ptr);
        ASSERT_EQ(result, ZE_RESULT_SUCCESS);
    }

    AppendKernelXeHpcTestInput input;
};

using CommandListAppendLaunchMultiKernelEventDisabledSinglePacketXeHpcCore = Test<CommandListAppendLaunchMultiKernelEventFixture<0>>;

HWTEST2_F(CommandListAppendLaunchMultiKernelEventDisabledSinglePacketXeHpcCore,
          givenHwSupportsSystemFenceWhenKernelUsingDeviceMemoryAllocationsAndEventHostSignalScopeThenExpectsSystemFenceNotUsed, IsXeHpcCore) {
    testHostSignalScopeDeviceMemoryAppendMultiKernelCopy<gfxCoreFamily>(input);
}

HWTEST2_F(CommandListAppendLaunchMultiKernelEventDisabledSinglePacketXeHpcCore,
          givenHwSupportsSystemFenceWhenKernelUsingUsmHostMemoryAllocationsAndEventHostSignalScopeThenExpectsSystemFenceUsed, IsXeHpcCore) {
    testHostSignalScopeHostMemoryAppendMultiKernelCopy<gfxCoreFamily>(input);
}

using CommandListAppendLaunchMultiKernelEventEnabledSinglePacketXeHpcCore = Test<CommandListAppendLaunchMultiKernelEventFixture<1>>;

HWTEST2_F(CommandListAppendLaunchMultiKernelEventEnabledSinglePacketXeHpcCore,
          givenHwSupportsSystemFenceWhenKernelUsingDeviceMemoryAllocationsAndEventHostSignalScopeThenExpectsSystemFenceNotUsed, IsXeHpcCore) {
    testHostSignalScopeDeviceMemoryAppendMultiKernelCopy<gfxCoreFamily>(input);
}

HWTEST2_F(CommandListAppendLaunchMultiKernelEventEnabledSinglePacketXeHpcCore,
          givenHwSupportsSystemFenceWhenKernelUsingUsmHostMemoryAllocationsAndEventHostSignalScopeThenExpectsSystemFenceUsed, IsXeHpcCore) {
    testHostSignalScopeHostMemoryAppendMultiKernelCopy<gfxCoreFamily>(input);
}

} // namespace ult
} // namespace L0
