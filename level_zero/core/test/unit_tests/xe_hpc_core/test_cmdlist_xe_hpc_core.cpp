/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using CommandListAppendLaunchKernelXeHpcCore = Test<ModuleFixture>;
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
    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    {
        VariableBackup<EngineGroupType> engineGroupType{&pCommandList->engineGroupType};
        VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
        engineGroupType = EngineGroupType::RenderCompute;
        hwRevId = hwConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
        result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

        ze_group_count_t groupCount1{1, 1, 1};
        result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount1, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
}

using CommandListStatePrefetchXeHpcCore = Test<ModuleFixture>;

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenUnifiedSharedMemoryWhenPrefetchApiCalledThenDontSetMemPrefetch, IsXeHpcCore) {
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto res = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    auto ret = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    EXPECT_FALSE(memoryManager->setMemPrefetchCalled);

    context->freeMem(ptr);
}

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsWhenPrefetchApiCalledThenDontCallSetMemPrefetchByDefault, IsXeHpcCore) {
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
    auto res = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    auto ret = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    EXPECT_FALSE(memoryManager->setMemPrefetchCalled);

    context->freeMem(ptr);
}

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsSetWhenPrefetchApiCalledOnUnifiedSharedMemoryThenCallSetMemPrefetch, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(1);
    DebugManager.flags.UseKmdMigration.set(1);

    EXPECT_EQ(0b0001u, neoDevice->deviceBitfield.to_ulong());

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto res = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    auto ret = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    EXPECT_TRUE(memoryManager->setMemPrefetchCalled);
    EXPECT_EQ(0u, memoryManager->memPrefetchSubDeviceId);

    context->freeMem(ptr);
}

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsSetWhenPrefetchApiCalledOnUnifiedSharedMemoryThenCallSetMemPrefetchOnTheAssociatedDevice, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(1);
    DebugManager.flags.UseKmdMigration.set(1);

    neoDevice->deviceBitfield = 0b0010;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto res = context->allocSharedMem(device->toHandle(), &deviceDesc, &hostDesc, size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    auto ret = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    EXPECT_TRUE(memoryManager->setMemPrefetchCalled);
    EXPECT_EQ(1u, memoryManager->memPrefetchSubDeviceId);

    context->freeMem(ptr);
}

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsSetWhenPrefetchApiCalledOnUnifiedDeviceMemoryThenDontCallSetMemPrefetch, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(1);
    DebugManager.flags.UseKmdMigration.set(1);

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(), &deviceDesc, size, alignment, &ptr);
    EXPECT_NE(nullptr, ptr);

    auto ret = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    EXPECT_FALSE(memoryManager->setMemPrefetchCalled);

    context->freeMem(ptr);
}

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenAppendMemoryPrefetchForKmdMigratedSharedAllocationsSetWhenPrefetchApiCalledOnUnifiedHostMemoryThenDontCallSetMemPrefetch, IsXeHpcCore) {
    DebugManagerStateRestore restore;
    DebugManager.flags.AppendMemoryPrefetchForKmdMigratedSharedAllocations.set(1);
    DebugManager.flags.UseKmdMigration.set(1);

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_host_mem_alloc_desc_t hostDesc = {};
    context->allocHostMem(&hostDesc, size, alignment, &ptr);
    EXPECT_NE(nullptr, ptr);

    auto ret = pCommandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    EXPECT_FALSE(memoryManager->setMemPrefetchCalled);

    context->freeMem(ptr);
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
    commandList->appendEventForProfiling(event->toHandle(), false, false);

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
    commandList->appendSignalEventPostWalker(event->toHandle(), false);

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
} // namespace ult
} // namespace L0
