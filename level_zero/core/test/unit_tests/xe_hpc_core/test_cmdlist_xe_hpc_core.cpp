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
    bool isCooperative = true;
    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    {
        VariableBackup<EngineGroupType> engineGroupType{&pCommandList->engineGroupType};
        VariableBackup<unsigned short> hwRevId{&hwInfo.platform.usRevId};
        engineGroupType = EngineGroupType::RenderCompute;
        hwRevId = hwConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
        result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, isCooperative);
        EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);

        ze_group_count_t groupCount1{1, 1, 1};
        result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount1, nullptr, false, false, isCooperative);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    }
}

using CommandListStatePrefetchXeHpcCore = Test<ModuleFixture>;

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenDebugFlagSetWhenPrefetchApiCalledThenProgramStatePrefetch, IsXeHpcCore) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    DebugManagerStateRestore restore;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    constexpr size_t size = MemoryConstants::cacheLineSize * 2;
    constexpr size_t alignment = MemoryConstants::pageSize64k;
    constexpr size_t offset = MemoryConstants::cacheLineSize;
    constexpr uint32_t mocsIndexForL3 = (2 << 1);
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(), &deviceDesc, size + offset, alignment, &ptr);
    EXPECT_NE(nullptr, ptr);

    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.usRevId |= FamilyType::pvcBaseDieRevMask;

    auto cmdListBaseOffset = pCommandList->commandContainer.getCommandStream()->getUsed();

    {
        auto ret = pCommandList->appendMemoryPrefetch(ptrOffset(ptr, offset), size);
        EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

        EXPECT_EQ(cmdListBaseOffset, pCommandList->commandContainer.getCommandStream()->getUsed());
    }

    {
        DebugManager.flags.AddStatePrefetchCmdToMemoryPrefetchAPI.set(1);

        auto ret = pCommandList->appendMemoryPrefetch(ptrOffset(ptr, offset), size);
        EXPECT_EQ(ZE_RESULT_SUCCESS, ret);

        EXPECT_EQ(cmdListBaseOffset + sizeof(STATE_PREFETCH), pCommandList->commandContainer.getCommandStream()->getUsed());

        auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(ptrOffset(pCommandList->commandContainer.getCommandStream()->getCpuBase(), cmdListBaseOffset));

        EXPECT_EQ(statePrefetchCmd->getAddress(), reinterpret_cast<uint64_t>(ptrOffset(ptr, offset)));
        EXPECT_FALSE(statePrefetchCmd->getKernelInstructionPrefetch());
        EXPECT_EQ(mocsIndexForL3, statePrefetchCmd->getMemoryObjectControlState());
        EXPECT_EQ(1u, statePrefetchCmd->getPrefetchSize());

        EXPECT_EQ(reinterpret_cast<uint64_t>(ptr), pCommandList->commandContainer.getResidencyContainer().back()->getGpuAddress());
    }

    context->freeMem(ptr);
}

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

HWTEST2_F(CommandListStatePrefetchXeHpcCore, givenCommandBufferIsExhaustedWhenPrefetchApiCalledThenProgramStatePrefetch, IsXeHpcCore) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;
    using MI_BATCH_BUFFER_END = typename FamilyType::MI_BATCH_BUFFER_END;

    DebugManagerStateRestore restore;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    constexpr size_t size = MemoryConstants::cacheLineSize * 2;
    constexpr size_t alignment = MemoryConstants::pageSize64k;
    constexpr size_t offset = MemoryConstants::cacheLineSize;
    constexpr uint32_t mocsIndexForL3 = (2 << 1);
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    context->allocDeviceMem(device->toHandle(), &deviceDesc, size + offset, alignment, &ptr);
    EXPECT_NE(nullptr, ptr);

    auto hwInfo = device->getNEODevice()->getRootDeviceEnvironment().getMutableHardwareInfo();
    hwInfo->platform.usRevId |= FamilyType::pvcBaseDieRevMask;

    auto firstBatchBufferAllocation = pCommandList->commandContainer.getCommandStream()->getGraphicsAllocation();

    auto useSize = pCommandList->commandContainer.getCommandStream()->getAvailableSpace();
    useSize -= sizeof(MI_BATCH_BUFFER_END);
    pCommandList->commandContainer.getCommandStream()->getSpace(useSize);

    DebugManager.flags.AddStatePrefetchCmdToMemoryPrefetchAPI.set(1);

    auto ret = pCommandList->appendMemoryPrefetch(ptrOffset(ptr, offset), size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, ret);
    auto secondBatchBufferAllocation = pCommandList->commandContainer.getCommandStream()->getGraphicsAllocation();

    EXPECT_NE(firstBatchBufferAllocation, secondBatchBufferAllocation);

    auto statePrefetchCmd = reinterpret_cast<STATE_PREFETCH *>(pCommandList->commandContainer.getCommandStream()->getCpuBase());

    EXPECT_EQ(statePrefetchCmd->getAddress(), reinterpret_cast<uint64_t>(ptrOffset(ptr, offset)));
    EXPECT_FALSE(statePrefetchCmd->getKernelInstructionPrefetch());
    EXPECT_EQ(mocsIndexForL3, statePrefetchCmd->getMemoryObjectControlState());
    EXPECT_EQ(1u, statePrefetchCmd->getPrefetchSize());

    NEO::ResidencyContainer::iterator it = pCommandList->commandContainer.getResidencyContainer().end();
    it--;
    EXPECT_EQ(secondBatchBufferAllocation->getGpuAddress(), (*it)->getGpuAddress());
    it--;
    EXPECT_EQ(reinterpret_cast<uint64_t>(ptr), (*it)->getGpuAddress());

    context->freeMem(ptr);
}

using CommandListEventFenceTestsXeHpcCore = Test<ModuleFixture>;

HWTEST2_F(CommandListEventFenceTestsXeHpcCore, givenCommandListWithProfilingEventAfterCommandOnPvcRev00ThenMiFenceIsNotAdded, IsXeHpcCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_MEM_FENCE = typename FamilyType::MI_MEM_FENCE;

    if (defaultHwInfo->platform.eProductFamily != IGFX_PVC) {
        GTEST_SKIP();
    }

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    auto hwInfo = commandList->commandContainer.getDevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo->platform.usRevId = 0x00;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    commandList->appendEventForProfiling(event->toHandle(), false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_MEM_FENCE *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListEventFenceTestsXeHpcCore, givenCommandListWithProfilingEventAfterCommandOnPvcRev03ThenMiFenceIsAdded, IsXeHpcCore) {
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
    commandList->appendEventForProfiling(event->toHandle(), false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_MEM_FENCE *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListEventFenceTestsXeHpcCore, givenCommandListWithRegularEventAfterCommandOnPvcRev03ThenMiFenceIsAdded, IsXeHpcCore) {
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
    commandList->appendSignalEventPostWalker(event->toHandle());

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

using CommandListAppendBarrierXeHpcCore = Test<DeviceFixture>;

HWTEST2_F(CommandListAppendBarrierXeHpcCore, givenCommandListWhenAppendingBarrierThenPipeControlIsProgrammedAndHdcAndUnTypedFlushesAreSet, IsPVC) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::RenderCompute, 0u);
    ze_result_t returnValue = commandList->appendBarrier(nullptr, 0, nullptr);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    // PC for STATE_BASE_ADDRESS from list initialization
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;

    // PC for appendBarrier
    itor = find<PIPE_CONTROL *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*itor);
    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
    EXPECT_TRUE(pipeControlCmd->getUnTypedDataPortCacheFlush());
}

} // namespace ult
} // namespace L0
