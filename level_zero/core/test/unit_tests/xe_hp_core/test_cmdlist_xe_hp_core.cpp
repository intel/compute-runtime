/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/l3_range.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using CommandListCreate = Test<DeviceFixture>;
using CommandListAppendLaunchKernel = Test<ModuleFixture>;

using CommandListAppendLaunchKernelWithAtomics = Test<ModuleFixture>;

HWTEST2_F(CommandListAppendLaunchKernelWithAtomics, givenKernelWithNoGlobalAtomicsThenLastSentGlobalAtomicsInContainerStaysFalse, IsXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableWalkerPartition.set(1);
    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.useGlobalAtomics = false;
    EXPECT_FALSE(pCommandList->commandContainer.lastSentUseGlobalAtomics);

    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(pCommandList->commandContainer.lastSentUseGlobalAtomics);
}

HWTEST2_F(CommandListAppendLaunchKernelWithAtomics, givenKernelWithGlobalAtomicsThenLastSentGlobalAtomicsInContainerIsSetToTrue, IsXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableWalkerPartition.set(1);
    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.useGlobalAtomics = true;
    EXPECT_FALSE(pCommandList->commandContainer.lastSentUseGlobalAtomics);

    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(pCommandList->commandContainer.lastSentUseGlobalAtomics);
}

HWTEST2_F(CommandListAppendLaunchKernelWithAtomics, givenKernelWithGlobalAtomicsAndLastSentGlobalAtomicsInContainerTrueThenLastSentGlobalAtomicsStaysTrue, IsXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableWalkerPartition.set(1);
    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.useGlobalAtomics = true;
    pCommandList->commandContainer.lastSentUseGlobalAtomics = true;

    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_TRUE(pCommandList->commandContainer.lastSentUseGlobalAtomics);
}

HWTEST2_F(CommandListAppendLaunchKernelWithAtomics, givenKernelWithNoGlobalAtomicsAndLastSentGlobalAtomicsInContainerTrueThenLastSentGlobalAtomicsIsSetToFalse, IsXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableWalkerPartition.set(1);
    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.useGlobalAtomics = false;
    pCommandList->commandContainer.lastSentUseGlobalAtomics = true;

    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(pCommandList->commandContainer.lastSentUseGlobalAtomics);
}

HWTEST2_F(CommandListAppendLaunchKernelWithAtomics, givenKernelWithGlobalAtomicsAndNoImplicitScalingThenLastSentGlobalAtomicsInContainerStaysFalse, IsXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableWalkerPartition.set(0);
    Mock<::L0::Kernel> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &kernelAttributes = kernel.immutableData.kernelDescriptor->kernelAttributes;
    kernelAttributes.flags.useGlobalAtomics = true;
    EXPECT_FALSE(pCommandList->commandContainer.lastSentUseGlobalAtomics);

    result = pCommandList->appendLaunchKernelWithParams(kernel.toHandle(), &groupCount, nullptr, false, false, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_FALSE(pCommandList->commandContainer.lastSentUseGlobalAtomics);
}

HWTEST2_F(CommandListCreate, WhenCreatingCommandListThenBindingTablePoolAllocAddedToBatchBuffer, IsXeHpCore) {
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    auto &commandContainer = commandList->commandContainer;
    auto gmmHelper = commandContainer.getDevice()->getGmmHelper();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itor = find<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    {

        uint32_t streamBuffer[50] = {};
        NEO::LinearStream linearStream(streamBuffer, sizeof(streamBuffer));

        NEO::StateBaseAddressHelper<FamilyType>::programBindingTableBaseAddress(
            linearStream,
            *commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE),
            gmmHelper);

        auto expectedCommand = reinterpret_cast<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(streamBuffer);

        auto programmedCommand = genCmdCast<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(*itor);

        EXPECT_EQ(0, memcmp(expectedCommand, programmedCommand, sizeof(_3DSTATE_BINDING_TABLE_POOL_ALLOC)));
    }
}

HWTEST2_F(CommandListCreate, givenNotCopyCommandListWhenProfilingEventBeforeCommandThenStoreRegMemAdded, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_LOAD_REGISTER_REG = typename GfxFamily::MI_LOAD_REGISTER_REG;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    commandList->appendEventForProfiling(event->toHandle(), true);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmd->getSourceRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    itor++;
    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmd->getSourceRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
}

HWTEST2_F(CommandListCreate, givenNotCopyCommandListWhenProfilingEventAfterCommandThenPipeControlAndStoreRegMemAdded, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_LOAD_REGISTER_REG = typename GfxFamily::MI_LOAD_REGISTER_REG;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    commandList->appendEventForProfiling(event->toHandle(), false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmd->getSourceRegisterAddress(), REG_GLOBAL_TIMESTAMP_LDW);
    itor++;
    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmd->getSourceRegisterAddress(), GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenProfilingEventThenStoreRegCommandIsAdded, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
    commandList->appendEventForProfiling(event->toHandle(), false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenAllocationsWhenAppendRangesBarrierThenL3ControlIsProgrammed, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;
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
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    EXPECT_EQ(cmdList.end(), ++itor);
}

HWTEST2_F(CommandListCreate, givenAllocationWithSizeTooBigForL3ControlWhenAppendRangesBarrierThenTwoL3ControlAreProgrammed, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    uint64_t gpuAddress = 0x2000;
    void *buffer = reinterpret_cast<void *>(gpuAddress);
    size_t size = NEO::L3Range::maxSingleRange * (NEO::maxFlushSubrangeCount + 1);

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
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    EXPECT_NE(cmdList.end(), ++itor);
}

HWTEST2_F(CommandListCreate, givenRangeSizeTwiceBiggerThanAllocWhenAppendRangesBarrierThenL3ControlIsNotProgrammed, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    uint64_t gpuAddress = 0x1200;
    void *buffer = reinterpret_cast<void *>(gpuAddress);
    size_t size = 0x1000;

    NEO::MockGraphicsAllocation mockAllocation(buffer, gpuAddress, size);
    NEO::SvmAllocationData allocData(0);
    allocData.size = size;
    allocData.gpuAllocations.addAllocation(&mockAllocation);
    device->getDriverHandle()->getSvmAllocsManager()->insertSVMAlloc(allocData);
    const void *ranges[] = {buffer};
    const size_t sizes[] = {2 * size};
    commandList->applyMemoryRangesBarrier(1, sizes, ranges);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}
HWTEST2_F(CommandListCreate, givenRangeNotInSvmManagerThanAllocWhenAppendRangesBarrierThenL3ControlIsNotProgrammed, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    uint64_t gpuAddress = 0x1200;
    void *buffer = reinterpret_cast<void *>(gpuAddress);
    size_t size = 0x1000;

    const void *ranges[] = {buffer};
    const size_t sizes[] = {size};
    commandList->applyMemoryRangesBarrier(1, sizes, ranges);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), commandList->commandContainer.getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenRangeNotAlignedToPageWhenAppendRangesBarrierThenCommandAdressIsAligned, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;
    using L3_FLUSH_ADDRESS_RANGE = typename GfxFamily::L3_FLUSH_ADDRESS_RANGE;
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
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto programmedCommand = genCmdCast<L3_CONTROL *>(*itor);
    programmedCommand++;
    L3_FLUSH_ADDRESS_RANGE *l3Ranges = reinterpret_cast<L3_FLUSH_ADDRESS_RANGE *>(programmedCommand);
    EXPECT_EQ(l3Ranges->getAddress(), alignDown(gpuAddress, MemoryConstants::pageSize));
}

HWTEST2_F(CommandListCreate, givenRangeBetweenTwoPagesWhenAppendRangesBarrierThenAddressMaskIsCorrect, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using L3_CONTROL = typename GfxFamily::L3_CONTROL;
    using L3_FLUSH_ADDRESS_RANGE = typename GfxFamily::L3_FLUSH_ADDRESS_RANGE;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::Copy, 0u);
    uint64_t gpuAddress = 2 * MemoryConstants::pageSize + MemoryConstants::pageSize / 2;
    void *buffer = reinterpret_cast<void *>(gpuAddress);
    size_t size = MemoryConstants::pageSize / 2 + 1;

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
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto programmedCommand = genCmdCast<L3_CONTROL *>(*itor);
    programmedCommand++;
    L3_FLUSH_ADDRESS_RANGE *l3Ranges = reinterpret_cast<L3_FLUSH_ADDRESS_RANGE *>(programmedCommand);
    EXPECT_EQ(l3Ranges->getAddressMask(), NEO::L3Range::getMaskFromSize(2 * MemoryConstants::pageSize));
}

HWTEST2_F(CommandListAppendLaunchKernel, givenEventWhenInvokingAppendLaunchKernelThenPostSyncIsAdded, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    createKernel();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    EXPECT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    bool postSyncFound = false;
    auto gpuAddress = event->getGpuAddress(device);

    auto itorPS = findAll<WALKER_TYPE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPS.size());

    for (auto it : itorPS) {
        auto cmd = genCmdCast<WALKER_TYPE *>(*it);
        auto &postSync = cmd->getPostSync();
        EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
        EXPECT_EQ(gpuAddress, postSync.getDestinationAddress());
        EXPECT_EQ(true, postSync.getL3Flush());
        postSyncFound = true;
    }
    EXPECT_TRUE(postSyncFound);
}

HWTEST2_F(CommandListAppendLaunchKernel, givenTimestampEventWhenInvokingAppendLaunchKernelThenPostSyncIsAdded, IsXeHpCore) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;

    createKernel();
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(L0::CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    auto usedSpaceBefore = commandList->commandContainer.getCommandStream()->getUsed();
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc));
    auto event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));

    ze_group_count_t groupCount{1, 1, 1};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), &groupCount, event->toHandle(), 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->commandContainer.getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    EXPECT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    bool postSyncFound = false;
    auto gpuAddress = event->getGpuAddress(device);

    auto itorPS = findAll<WALKER_TYPE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, itorPS.size());

    for (auto it : itorPS) {
        auto cmd = genCmdCast<WALKER_TYPE *>(*it);
        auto &postSync = cmd->getPostSync();
        EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_TIMESTAMP, postSync.getOperation());
        EXPECT_EQ(gpuAddress, postSync.getDestinationAddress());
        EXPECT_EQ(false, postSync.getL3Flush());
        postSyncFound = true;
    }
    EXPECT_TRUE(postSyncFound);
}

} // namespace ult
} // namespace L0
