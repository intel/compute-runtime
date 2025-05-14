/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/l3_range.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/xe_hpg_core/test_traits_xe_hpg_core.h"

#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/xe_hpg_core/cmdlist_xe_hpg_core.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

namespace L0 {
namespace ult {

using CommandListCreate = Test<DeviceFixture>;

HWTEST2_F(CommandListCreate, WhenCreatingCommandListThenBindingTablePoolAllocAddedToBatchBuffer, IsXeHpgCore) {
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableStateBaseAddressTracking.set(0);
    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);

    debugManager.flags.UseExternalAllocatorForSshAndDsh.set(0);
    device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.reset();

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();
    auto gmmHelper = commandContainer.getDevice()->getGmmHelper();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList,
                                                      ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0),
                                                      usedSpaceAfter));

    auto itor = find<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    {

        uint32_t streamBuffer[50] = {};
        NEO::LinearStream linearStream(streamBuffer, sizeof(streamBuffer));

        NEO::StateBaseAddressHelper<FamilyType>::programBindingTableBaseAddress(
            linearStream,
            *commandContainer.getIndirectHeap(NEO::HeapType::surfaceState),
            gmmHelper);

        auto expectedCommand = reinterpret_cast<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(streamBuffer);

        auto programmedCommand = genCmdCast<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(*itor);

        EXPECT_EQ(0, memcmp(expectedCommand, programmedCommand, sizeof(_3DSTATE_BINDING_TABLE_POOL_ALLOC)));
    }
}

HWTEST2_F(CommandListCreate, givenNotCopyCommandListWhenProfilingEventBeforeCommandThenStoreRegMemAdded, IsXeHpgCore) {

    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    commandList->appendEventForProfiling(event.get(), nullptr, true, false, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<MI_LOAD_REGISTER_REG *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmd->getSourceRegisterAddress(), RegisterOffsets::globalTimestampLdw);
    itor++;
    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmd->getSourceRegisterAddress(), RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
}

HWTEST2_F(CommandListCreate, givenNotCopyCommandListWhenProfilingEventAfterCommandThenPipeControlAndStoreRegMemAdded, IsXeHpgCore) {

    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    commandList->appendEventForProfiling(event.get(), nullptr, false, false, false, false);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));

    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    itor++;
    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmd->getSourceRegisterAddress(), RegisterOffsets::globalTimestampLdw);
    itor++;
    itor = find<MI_LOAD_REGISTER_REG *>(itor, cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    cmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*itor);
    EXPECT_EQ(cmd->getSourceRegisterAddress(), RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenProfilingEventThenStoreRegCommandIsAdded, IsXeHpgCore) {

    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    commandList->appendEventForProfiling(event.get(), nullptr, false, false, false, true);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenAllocationsWhenAppendRangesBarrierThenL3ControlIsProgrammed, IsXeHpgCore) {

    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
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
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    EXPECT_EQ(cmdList.end(), ++itor);
}

HWTEST2_F(CommandListCreate, givenAllocationWithSizeTooBigForL3ControlWhenAppendRangesBarrierThenTwoL3ControlAreProgrammed, IsXeHpgCore) {

    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
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
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    EXPECT_NE(cmdList.end(), ++itor);
}

HWTEST2_F(CommandListCreate, givenRangeSizeTwiceBiggerThanAllocWhenAppendRangesBarrierThenL3ControlIsNotProgrammed, IsXeHpgCore) {

    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
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
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}
HWTEST2_F(CommandListCreate, givenRangeNotInSvmManagerThanAllocWhenAppendRangesBarrierThenL3ControlIsNotProgrammed, IsXeHpgCore) {

    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    uint64_t gpuAddress = 0x1200;
    void *buffer = reinterpret_cast<void *>(gpuAddress);
    size_t size = 0x1000;

    const void *ranges[] = {buffer};
    const size_t sizes[] = {size};
    commandList->applyMemoryRangesBarrier(1, sizes, ranges);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenRangeNotAlignedToPageWhenAppendRangesBarrierThenCommandAddressIsAligned, IsXeHpgCore) {

    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
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
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto programmedCommand = genCmdCast<L3_CONTROL *>(*itor);
    programmedCommand++;
    L3_FLUSH_ADDRESS_RANGE *l3Ranges = reinterpret_cast<L3_FLUSH_ADDRESS_RANGE *>(programmedCommand);
    EXPECT_EQ(l3Ranges->getAddress(), alignDown(gpuAddress, MemoryConstants::pageSize));
}

HWTEST2_F(CommandListCreate, givenRangeBetweenTwoPagesWhenAppendRangesBarrierThenAddressMaskIsCorrect, IsXeHpgCore) {

    using L3_CONTROL = typename FamilyType::L3_CONTROL;
    using L3_FLUSH_ADDRESS_RANGE = typename FamilyType::L3_FLUSH_ADDRESS_RANGE;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
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
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<L3_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto programmedCommand = genCmdCast<L3_CONTROL *>(*itor);
    programmedCommand++;
    L3_FLUSH_ADDRESS_RANGE *l3Ranges = reinterpret_cast<L3_FLUSH_ADDRESS_RANGE *>(programmedCommand);
    EXPECT_EQ(l3Ranges->getAddressMask(), NEO::L3Range::getMaskFromSize(2 * MemoryConstants::pageSize));
}

template <GFXCORE_FAMILY gfxCoreFamily>
struct CommandListAdjustStateComputeMode : public WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>> {
    CommandListAdjustStateComputeMode() : WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>() {}
    using ::L0::CommandListCoreFamily<gfxCoreFamily>::finalStreamState;
    using ::L0::CommandListCoreFamily<gfxCoreFamily>::stateComputeModeTracking;
    using ::L0::CommandListCoreFamily<gfxCoreFamily>::updateStreamProperties;
};
struct ProgramAllFieldsInComputeMode {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (NEO::ToGfxCoreFamily<productFamily>::get() != IGFX_XE_HPG_CORE) {
            return false;
        } else {
            return !TestTraitsPlatforms<productFamily>::programOnlyChangedFieldsInComputeStateMode;
        }
    }
};

HWTEST2_F(CommandListCreate, GivenComputeModePropertiesWhenUpdateStreamPropertiesIsCalledTwiceThenWithoutTrackingFieldsChangedWithTrackingUpdatedClean, ProgramAllFieldsInComputeMode) {
    DebugManagerStateRestore restorer;
    Mock<::L0::KernelImp> kernel;
    auto &productHelper = device->getProductHelper();

    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();
    auto commandList = std::make_unique<CommandListAdjustStateComputeMode<FamilyType::gfxCoreFamily>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x100;
    const ze_group_count_t launchKernelArgs = {};
    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    if (commandList->stateComputeModeTracking) {
        EXPECT_FALSE(commandList->finalStreamState.stateComputeMode.isCoherencyRequired.isDirty);
        if (productHelper.isGrfNumReportedWithScm()) {
            EXPECT_NE(-1, commandList->finalStreamState.stateComputeMode.largeGrfMode.value);
        } else {
            EXPECT_EQ(-1, commandList->finalStreamState.stateComputeMode.largeGrfMode.value);
        }
    } else {
        EXPECT_TRUE(commandList->finalStreamState.stateComputeMode.isCoherencyRequired.isDirty);
        EXPECT_TRUE(commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    }
    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x80;
    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_TRUE(commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    EXPECT_FALSE(commandList->finalStreamState.stateComputeMode.isCoherencyRequired.isDirty);
}
struct ProgramDirtyFieldsInComputeMode {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (NEO::ToGfxCoreFamily<productFamily>::get() != IGFX_XE_HPG_CORE) {
            return false;
        } else {
            return TestTraitsPlatforms<productFamily>::programOnlyChangedFieldsInComputeStateMode;
        }
    }
};

HWTEST2_F(CommandListCreate, GivenComputeModePropertiesWhenUpdateStreamPropertiesIsCalledTwiceDirtyFieldsChangedAndWithTrackingIsCleanAfterFirstCall, ProgramDirtyFieldsInComputeMode) {
    DebugManagerStateRestore restorer;
    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();
    auto commandList = std::make_unique<CommandListAdjustStateComputeMode<FamilyType::gfxCoreFamily>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x100;
    const ze_group_count_t launchKernelArgs = {};
    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    if (commandList->stateComputeModeTracking) {
        EXPECT_FALSE(commandList->finalStreamState.stateComputeMode.isCoherencyRequired.isDirty);
        EXPECT_FALSE(commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    } else {
        EXPECT_TRUE(commandList->finalStreamState.stateComputeMode.isCoherencyRequired.isDirty);
        EXPECT_TRUE(commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    }
    const_cast<NEO::KernelDescriptor *>(&kernel.getKernelDescriptor())->kernelAttributes.numGrfRequired = 0x80;
    commandList->updateStreamProperties(kernel, false, launchKernelArgs, false);
    EXPECT_TRUE(commandList->finalStreamState.stateComputeMode.largeGrfMode.isDirty);
    EXPECT_FALSE(commandList->finalStreamState.stateComputeMode.isCoherencyRequired.isDirty);
}

using CommandListAppendLaunchKernelXeHpgCore = Test<ModuleFixture>;
HWTEST2_F(CommandListAppendLaunchKernelXeHpgCore, givenEventWhenAppendKernelIsCalledThenImmediateDataPostSyncIsAdded, IsXeHpgCore) {
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    DebugManagerStateRestore restorer;
    debugManager.flags.CompactL3FlushEventPacket.set(0);

    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(1, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::cooperativeCompute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<Event>(Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    auto usedSpaceBefore = commandList->getCmdContainer().getCommandStream()->getUsed();
    CmdListKernelLaunchParams launchParams = {};
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    EXPECT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    EXPECT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), usedSpaceBefore),
        usedSpaceAfter - usedSpaceBefore));

    auto gpuAddress = event->getGpuAddress(device);

    auto itorWalker = find<DefaultWalkerType *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorWalker);
    auto cmdWalker = genCmdCast<DefaultWalkerType *>(*itorWalker);
    auto &postSync = cmdWalker->getPostSync();
    EXPECT_EQ(POSTSYNC_DATA::OPERATION_WRITE_IMMEDIATE_DATA, postSync.getOperation());
    EXPECT_EQ(gpuAddress, postSync.getDestinationAddress());

    gpuAddress += event->getSinglePacketSize();
    auto itorPC = findAll<PIPE_CONTROL *>(itorWalker, cmdList.end());
    ASSERT_NE(0u, itorPC.size());
    uint32_t postSyncCount = 0u;
    for (auto it : itorPC) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
            postSyncCount++;
        }
    }
    EXPECT_EQ(1u, postSyncCount);
}

} // namespace ult
} // namespace L0
