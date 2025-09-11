/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/state_base_address_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_encoder.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "test_traits_common.h"

using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

namespace L0 {
namespace ult {

using CommandListTests = Test<DeviceFixture>;
HWCMDTEST_F(IGFX_XE_HP_CORE, CommandListTests, whenCommandListIsCreatedThenPCAndStateBaseAddressCmdsAreAddedAndCorrectlyProgrammed) {

    auto &compilerProductHelper = device->getNEODevice()->getCompilerProductHelper();
    auto isHeaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    if (isHeaplessEnabled) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableStateBaseAddressTracking.set(0);
    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto bindlessHeapsHelper = device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.get();

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
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorPc = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPc);
    auto cmdPc = genCmdCast<PIPE_CONTROL *>(*itorPc);

    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*cmdPc));
    EXPECT_TRUE(cmdPc->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(cmdPc->getCommandStreamerStallEnable());

    auto itor = find<STATE_BASE_ADDRESS *>(itorPc, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);

    if (device->getNEODevice()->getDeviceInfo().imageSupport) {
        auto dsh = commandContainer.getIndirectHeap(NEO::HeapType::dynamicState);
        EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
        if (bindlessHeapsHelper) {
            EXPECT_EQ(bindlessHeapsHelper->getGlobalHeapsBase(), cmdSba->getDynamicStateBaseAddress());
            EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, cmdSba->getDynamicStateBufferSize());
        } else {
            EXPECT_EQ(dsh->getHeapGpuBase(), cmdSba->getDynamicStateBaseAddress());
            EXPECT_EQ(dsh->getHeapSizeInPages(), cmdSba->getDynamicStateBufferSize());
        }
    } else {
        EXPECT_FALSE(cmdSba->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(cmdSba->getDynamicStateBufferSizeModifyEnable());
    }

    if (bindlessHeapsHelper) {
        EXPECT_TRUE(cmdSba->getBindlessSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(bindlessHeapsHelper->getGlobalHeapsBase(), cmdSba->getBindlessSurfaceStateBaseAddress());
    } else {
        auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::surfaceState);
        EXPECT_TRUE(cmdSba->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(ssh->getHeapGpuBase(), cmdSba->getSurfaceStateBaseAddress());
    }

    EXPECT_EQ(gmmHelper->getL1EnabledMOCS(), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListTests, whenCommandListIsCreatedAndProgramExtendedPipeControlPriorToNonPipelinedStateCommandIsEnabledThenPCAndStateBaseAddressCmdsAreAddedAndCorrectlyProgrammed, IsAtLeastXeCore) {

    auto &compilerProductHelper = device->getNEODevice()->getCompilerProductHelper();
    auto isHeaplessEnabled = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo);
    if (isHeaplessEnabled) {
        GTEST_SKIP();
    }

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableStateBaseAddressTracking.set(0);
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(1);
    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);
    debugManager.flags.SelectCmdListHeapAddressModel.set(0);

    auto bindlessHeapsHelper = device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[device->getNEODevice()->getRootDeviceIndex()]->bindlessHeapsHelper.get();

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
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorPc = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPc);
    auto cmdPc = genCmdCast<PIPE_CONTROL *>(*itorPc);

    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*cmdPc));
    EXPECT_TRUE(cmdPc->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(cmdPc->getCommandStreamerStallEnable());

    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::isPipeControlExtendedPriorToNonPipelinedStateCommandSupported) {
        EXPECT_TRUE(cmdPc->getAmfsFlushEnable());
        EXPECT_TRUE(cmdPc->getInstructionCacheInvalidateEnable());
        EXPECT_TRUE(cmdPc->getConstantCacheInvalidationEnable());
        EXPECT_TRUE(cmdPc->getStateCacheInvalidationEnable());

        if constexpr (TestTraits<FamilyType::gfxCoreFamily>::isUnTypedDataPortCacheFlushSupported) {
            EXPECT_TRUE(cmdPc->getUnTypedDataPortCacheFlush());
        }
    }

    auto itor = find<STATE_BASE_ADDRESS *>(itorPc, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    if (device->getNEODevice()->getDeviceInfo().imageSupport) {
        auto dsh = commandContainer.getIndirectHeap(NEO::HeapType::dynamicState);
        EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
        if (bindlessHeapsHelper) {
            EXPECT_EQ(bindlessHeapsHelper->getGlobalHeapsBase(), cmdSba->getDynamicStateBaseAddress());
            EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, cmdSba->getDynamicStateBufferSize());
        } else {
            EXPECT_EQ(dsh->getHeapGpuBase(), cmdSba->getDynamicStateBaseAddress());
            EXPECT_EQ(dsh->getHeapSizeInPages(), cmdSba->getDynamicStateBufferSize());
        }
    } else {
        EXPECT_FALSE(cmdSba->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(cmdSba->getDynamicStateBufferSizeModifyEnable());
    }

    if (bindlessHeapsHelper) {
        EXPECT_TRUE(cmdSba->getBindlessSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(bindlessHeapsHelper->getGlobalHeapsBase(), cmdSba->getBindlessSurfaceStateBaseAddress());
    } else {
        auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::surfaceState);
        EXPECT_TRUE(cmdSba->getSurfaceStateBaseAddressModifyEnable());
        EXPECT_EQ(ssh->getHeapGpuBase(), cmdSba->getSurfaceStateBaseAddress());
    }

    EXPECT_EQ(gmmHelper->getL1EnabledMOCS(), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());
}

using CommandListTestsReserveSize = Test<DeviceFixture>;
HWTEST2_F(CommandListTestsReserveSize, givenCommandListWhenGetReserveSshSizeThen16slotSpaceReturned, IsHeapfulRequiredAndAtLeastXeCore) {
    L0::CommandListCoreFamily<FamilyType::gfxCoreFamily> commandList(1u);
    commandList.initialize(device, NEO::EngineGroupType::compute, 0u);

    EXPECT_EQ(commandList.getReserveSshSize(), (16 * 2 + 1) * 2 * sizeof(typename FamilyType::RENDER_SURFACE_STATE));
}

using CommandListAppendLaunchKernel = Test<ModuleFixture>;
HWTEST2_F(CommandListAppendLaunchKernel, givenVariousKernelsWhenUpdateStreamPropertiesIsCalledThenRequiredStateFinalStateAndCommandsToPatchAreCorrectlySet, IsHeapfulRequiredAndAtLeastXeCore) {
    DebugManagerStateRestore restorer;

    debugManager.flags.AllowPatchingVfeStateInCommandLists.set(1);

    Mock<::L0::KernelImp> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::KernelImp> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(-1, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(-1, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());

    auto &productHelper = device->getProductHelper();
    int32_t expectedDispatchAllWalkerEnable = productHelper.isComputeDispatchAllWalkerEnableInCfeStateRequired(device->getHwInfo()) ? 0 : -1;

    const ze_group_count_t launchKernelArgs = {};
    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);

    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);
    expectedDispatchAllWalkerEnable = expectedDispatchAllWalkerEnable != -1 ? 1 : expectedDispatchAllWalkerEnable;
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);
    expectedDispatchAllWalkerEnable = expectedDispatchAllWalkerEnable != -1 ? 0 : expectedDispatchAllWalkerEnable;
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    expectedDispatchAllWalkerEnable = expectedDispatchAllWalkerEnable != -1 ? 1 : expectedDispatchAllWalkerEnable;
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    size_t expectedCommandsToPatch = expectedDispatchAllWalkerEnable != -1 ? 1 : 0;
    EXPECT_EQ(expectedCommandsToPatch, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);
    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    expectedCommandsToPatch = expectedCommandsToPatch != 0 ? 2 : 0;
    EXPECT_EQ(expectedCommandsToPatch, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);
    expectedDispatchAllWalkerEnable = expectedDispatchAllWalkerEnable != -1 ? 0 : expectedDispatchAllWalkerEnable;
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    expectedDispatchAllWalkerEnable = expectedDispatchAllWalkerEnable != -1 ? 1 : expectedDispatchAllWalkerEnable;
    EXPECT_EQ(expectedDispatchAllWalkerEnable, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    expectedCommandsToPatch = expectedCommandsToPatch != 0 ? 1 : 0;
    EXPECT_EQ(expectedCommandsToPatch, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    EXPECT_EQ(-1, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(-1, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenVariousKernelsAndPatchingDisallowedWhenUpdateStreamPropertiesIsCalledThenCommandsToPatchAreEmpty, IsHeapfulRequiredAndAtLeastXeCore) {
    DebugManagerStateRestore restorer;

    Mock<::L0::KernelImp> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::KernelImp> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    const ze_group_count_t launchKernelArgs = {};
    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);

    const auto &productHelper = device->getProductHelper();

    size_t expectedCmdsToPatch = productHelper.isComputeDispatchAllWalkerEnableInCfeStateRequired(device->getHwInfo()) ? 1 : 0;

    EXPECT_EQ(expectedCmdsToPatch, pCommandList->commandsToPatch.size());

    pCommandList->reset();

    debugManager.flags.AllowPatchingVfeStateInCommandLists.set(1);
    pCommandList->updateStreamProperties(defaultKernel, false, launchKernelArgs, false);
    pCommandList->updateStreamProperties(cooperativeKernel, true, launchKernelArgs, false);

    EXPECT_EQ(expectedCmdsToPatch, pCommandList->commandsToPatch.size());
    pCommandList->reset();
}

struct AppendKernelTestInput {
    DriverHandle *driver = nullptr;
    L0::Context *context = nullptr;
    L0::Device *device = nullptr;

    ze_event_pool_flags_t eventPoolFlags = 0;

    uint32_t packetOffsetMul = 1;

    bool useFirstEventPacketAddress = false;
};

template <int32_t compactL3FlushEventPacket, uint32_t multiTile>
struct CommandListAppendLaunchKernelCompactL3FlushEventFixture : public ModuleFixture {
    void setUp() {
        debugManager.flags.CompactL3FlushEventPacket.set(compactL3FlushEventPacket);
        debugManager.flags.SignalAllEventPackets.set(0);
        if constexpr (multiTile == 1) {
            debugManager.flags.CreateMultipleSubDevices.set(2);
            debugManager.flags.EnableImplicitScaling.set(1);
            arg.workloadPartition = true;
            arg.expectDcFlush = 2; // DC Flush multi-tile platforms require DC Flush + x-tile sync after implicit scaling DefaultWalkerType
            input.packetOffsetMul = 2;
        } else {
            arg.expectDcFlush = 1;
        }
        ModuleFixture::setUp();

        input.driver = driverHandle.get();
        input.context = context;
        input.device = device;
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendLaunchKernelAndL3Flush(AppendKernelTestInput &input, TestExpectedValues &arg) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using WalkerType = typename FamilyType::DefaultWalkerType;
        using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
        using OPERATION = typename PostSyncType::OPERATION;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

        Mock<::L0::KernelImp> kernel;
        auto module = std::unique_ptr<Module>(new Mock<Module>(input.device, nullptr));
        kernel.module = module.get();

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = input.eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(input.driver, input.context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, input.device, result));

        uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device);

        ze_group_count_t groupCount{1, 1, 1};
        CmdListKernelLaunchParams launchParams = {};
        result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        EXPECT_EQ(arg.expectedPacketsInUse, event->getPacketsInUse());
        EXPECT_EQ(arg.expectedKernelCount, event->getKernelCount());

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList, ptrOffset(commandList->commandContainer.getCommandStream()->getCpuBase(), 0),
            commandList->commandContainer.getCommandStream()->getUsed()));

        auto itorWalkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, itorWalkers.size());
        auto firstWalker = itorWalkers[0];

        auto walker = genCmdCast<WalkerType *>(*firstWalker);
        auto &postSync = walker->getPostSync();

        EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), postSync.getOperation());
        EXPECT_EQ(firstKernelEventAddress, postSync.getDestinationAddress());

        uint64_t l3FlushPostSyncAddress = event->getGpuAddress(input.device) + input.packetOffsetMul * event->getSinglePacketSize();
        if (input.useFirstEventPacketAddress) {
            l3FlushPostSyncAddress = event->getGpuAddress(input.device);
        }
        if (event->isUsingContextEndOffset()) {
            l3FlushPostSyncAddress += event->getContextEndOffset();
        }

        auto itorPipeControls = findAll<PIPE_CONTROL *>(firstWalker, cmdList.end());

        uint32_t postSyncPipeControls = 0;
        uint32_t dcFlushFound = 0;
        for (auto it : itorPipeControls) {
            auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
            if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                postSyncPipeControls++;
                EXPECT_EQ(l3FlushPostSyncAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
                EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
                if (arg.workloadPartition) {
                    EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                } else {
                    EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                }
            }
            if (cmd->getDcFlushEnable()) {
                dcFlushFound++;
            }
        }
        EXPECT_EQ(arg.expectedPostSyncPipeControls, postSyncPipeControls);
        EXPECT_EQ(arg.expectDcFlush, dcFlushFound);

        auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
        if constexpr (multiTile == 1) {
            EXPECT_EQ(3u, itorStoreDataImm.size());
        } else {
            EXPECT_EQ(0u, itorStoreDataImm.size());
        }
    }

    DebugManagerStateRestore restorer;

    AppendKernelTestInput input = {};
    TestExpectedValues arg = {};
};

using CommandListAppendLaunchKernelCompactL3FlushDisabledTest = Test<CommandListAppendLaunchKernelCompactL3FlushEventFixture<0, 0>>;

HWTEST2_F(CommandListAppendLaunchKernelCompactL3FlushDisabledTest,
          givenAppendKernelWithSignalScopeTimestampEventWhenComputeWalkerTimestampPostsyncAndL3ImmediatePostsyncUsedThenExpectComputeWalkerAndPipeControlPostsync,
          IsXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testAppendLaunchKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListAppendLaunchKernelCompactL3FlushDisabledTest,
          givenAppendKernelWithSignalScopeImmediateEventWhenComputeWalkerImmediatePostsyncAndL3ImmediatePostsyncUsedThenExpectComputeWalkerAndPipeControlPostsync,
          IsXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = input.device->isImplicitScalingCapable() ? 3 : 1;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = 0;

    testAppendLaunchKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

using CommandListAppendLaunchKernelCompactL3FlushEnabledTest = Test<CommandListAppendLaunchKernelCompactL3FlushEventFixture<1, 0>>;

HWTEST2_F(CommandListAppendLaunchKernelCompactL3FlushEnabledTest,
          givenAppendKernelWithSignalScopeTimestampEventWhenRegisterTimestampPostsyncUsedThenExpectNoComputeWalkerAndPipeControlPostsync,
          IsXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 1;
    arg.expectedPostSyncPipeControls = 0;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    input.useFirstEventPacketAddress = true;

    testAppendLaunchKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListAppendLaunchKernelCompactL3FlushEnabledTest,
          givenAppendKernelWithSignalScopeImmediateEventWhenL3ImmediatePostsyncUsedThenExpectPipeControlPostsync,
          IsXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 1;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    testAppendLaunchKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

using CommandListAppendLaunchKernelMultiTileCompactL3FlushDisabledTest = Test<CommandListAppendLaunchKernelCompactL3FlushEventFixture<0, 1>>;

HWTEST2_F(CommandListAppendLaunchKernelMultiTileCompactL3FlushDisabledTest,
          givenAppendMultiTileKernelWithSignalScopeTimestampEventWhenComputeWalkerTimestampPostsyncAndL3ImmediatePostsyncUsedThenExpectComputeWalkerAndPipeControlPostsync,
          IsXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 4;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testAppendLaunchKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListAppendLaunchKernelMultiTileCompactL3FlushDisabledTest,
          givenAppendMultiTileKernelWithSignalScopeImmediateEventWhenComputeWalkerImmediatePostsyncAndL3ImmediatePostsyncUsedThenExpectComputeWalkerAndPipeControlPostsync,
          IsXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 4;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 1;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = 0;

    testAppendLaunchKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

using CommandListAppendLaunchKernelMultiTileCompactL3FlushEnabledTest = Test<CommandListAppendLaunchKernelCompactL3FlushEventFixture<1, 1>>;

HWTEST2_F(CommandListAppendLaunchKernelMultiTileCompactL3FlushEnabledTest,
          givenAppendMultiTileKernelWithSignalScopeTimestampEventWhenRegisterTimestampPostsyncUsedThenExpectNoComputeWalkerAndPipeControlPostsync,
          IsXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedPostSyncPipeControls = 0;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    input.useFirstEventPacketAddress = true;

    testAppendLaunchKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListAppendLaunchKernelMultiTileCompactL3FlushEnabledTest,
          givenAppendMultiTileKernelWithSignalScopeImmediateEventWhenL3ImmediatePostsyncUsedThenExpectPipeControlPostsync,
          IsXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    testAppendLaunchKernelAndL3Flush<FamilyType::gfxCoreFamily>(input, arg);
}

template <uint32_t multiTile, uint32_t limitEventPacketes, uint32_t copyOnly>
struct CommandListSignalAllEventPacketFixture : public ModuleFixture {
    void setUp() {
        NEO::debugManager.flags.UseDynamicEventPacketsCount.set(1);
        NEO::debugManager.flags.SignalAllEventPackets.set(1);

        if constexpr (limitEventPacketes == 1) {
            NEO::debugManager.flags.UsePipeControlMultiKernelEventSync.set(1);
            NEO::debugManager.flags.CompactL3FlushEventPacket.set(1);
        } else {
            NEO::debugManager.flags.UsePipeControlMultiKernelEventSync.set(0);
            NEO::debugManager.flags.CompactL3FlushEventPacket.set(0);
        }
        if constexpr (multiTile == 1) {
            debugManager.flags.CreateMultipleSubDevices.set(2);
            debugManager.flags.EnableImplicitScaling.set(1);
        }
        ModuleFixture::setUp();

        this->module = std::make_unique<Mock<Module>>(device, nullptr);
        this->kernel.module = this->module.get();
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendKernel(ze_event_pool_flags_t eventPoolFlags) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using WalkerType = typename FamilyType::DefaultWalkerType;
        using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
        using OPERATION = typename PostSyncType::OPERATION;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto cmdStream = commandList->commandContainer.getCommandStream();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
        ASSERT_NE(nullptr, event.get());

        ze_group_count_t groupCount{1, 1, 1};
        CmdListKernelLaunchParams launchParams = {};
        size_t sizeBefore = cmdStream->getUsed();
        result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
        size_t sizeAfter = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), sizeBefore),
            (sizeAfter - sizeBefore)));

        uint32_t expectedWalkerPostSyncOp = 3;
        if (multiTile == 0 && eventPoolFlags == 0 && !eventPool->isImplicitScalingCapableFlagSet()) {
            expectedWalkerPostSyncOp = 1;
        }

        if (expectedWalkerPostSyncOp == 3 && eventPoolFlags == 0 && multiTile != 0) {
            expectedWalkerPostSyncOp = 1;
        }

        auto itorWalkers = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
        ASSERT_EQ(1u, itorWalkers.size());
        auto firstWalker = itorWalkers[0];

        auto walker = genCmdCast<WalkerType *>(*firstWalker);
        auto &postSync = walker->getPostSync();

        EXPECT_EQ(static_cast<OPERATION>(expectedWalkerPostSyncOp), postSync.getOperation());

        uint32_t extraCleanupStoreDataImm = 0;
        if (multiTile == 1 && NEO::ImplicitScalingDispatch<FamilyType>::getPipeControlStallRequired()) {
            extraCleanupStoreDataImm = 3;
        }

        auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(firstWalker, cmdList.end());
        if constexpr (limitEventPacketes == 1) {
            ASSERT_EQ(extraCleanupStoreDataImm, itorStoreDataImm.size());
        } else {
            uint32_t packetUsed = event->getPacketsInUse();
            uint32_t remainingPackets = event->getMaxPacketsCount() - packetUsed;
            remainingPackets /= commandList->partitionCount;
            ASSERT_EQ(remainingPackets + extraCleanupStoreDataImm, static_cast<uint32_t>(itorStoreDataImm.size()));

            uint64_t gpuAddress = event->getCompletionFieldGpuAddress(device);
            gpuAddress += (packetUsed * event->getSinglePacketSize());

            uint32_t startIndex = extraCleanupStoreDataImm;
            for (uint32_t i = startIndex; i < remainingPackets + extraCleanupStoreDataImm; i++) {
                auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
                EXPECT_EQ(gpuAddress, cmd->getAddress());
                EXPECT_FALSE(cmd->getStoreQword());
                EXPECT_EQ(2u, cmd->getDataDword0());
                if constexpr (multiTile == 1) {
                    EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                } else {
                    EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                }
                gpuAddress += (event->getSinglePacketSize() * commandList->partitionCount);
            }
        }
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendSignalEvent(ze_event_pool_flags_t eventPoolFlags) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
        using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto engineType = copyOnly == 1 ? NEO::EngineGroupType::copy : NEO::EngineGroupType::compute;
        auto result = commandList->initialize(device, engineType, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto cmdStream = commandList->commandContainer.getCommandStream();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
        ASSERT_NE(nullptr, event.get());

        size_t sizeBefore = cmdStream->getUsed();
        result = commandList->appendSignalEvent(event->toHandle(), false);
        size_t sizeAfter = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), sizeBefore),
            (sizeAfter - sizeBefore)));

        if constexpr (copyOnly == 1) {
            uint32_t flushCmdWaFactor = 1;
            NEO::EncodeDummyBlitWaArgs waArgs{false, &(device->getNEODevice()->getRootDeviceEnvironmentRef())};
            if (MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs) > 0) {
                flushCmdWaFactor++;
            }

            auto itorFlushDw = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

            uint32_t expectedFlushDw = event->getMaxPacketsCount() * flushCmdWaFactor;
            ASSERT_EQ(expectedFlushDw, itorFlushDw.size());

            uint64_t gpuAddress = event->getCompletionFieldGpuAddress(device);

            for (uint32_t i = 0; i < expectedFlushDw; i++) {
                auto cmd = genCmdCast<MI_FLUSH_DW *>(*itorFlushDw[i]);
                if (flushCmdWaFactor == 2) {
                    // even flush commands are WAs
                    if ((i & 1) == 0) {
                        continue;
                    }
                }
                EXPECT_EQ(gpuAddress, cmd->getDestinationAddress());
                EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
                gpuAddress += event->getSinglePacketSize();
            }

        } else {
            auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
            auto itorPipeControl = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

            uint64_t gpuAddress = event->getCompletionFieldGpuAddress(device);

            uint32_t extraSignalStoreDataImm = 0;
            if (eventPoolFlags == 0) {
                extraSignalStoreDataImm = 1; // used packet reset for "non-TS, non-signal scope on DC Flush platforms" events performed by SDI command, other resets are via PIPE_CONTROL w/postsync
            }

            if constexpr (limitEventPacketes == 1) {
                ASSERT_EQ(extraSignalStoreDataImm, itorStoreDataImm.size());
                if (extraSignalStoreDataImm == 1) {
                    auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[0]);
                    EXPECT_EQ(gpuAddress, cmd->getAddress());
                    EXPECT_FALSE(cmd->getStoreQword());
                    EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
                    if constexpr (multiTile == 1) {
                        EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                    } else {
                        EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                    }
                } else {
                    uint32_t postSyncPipeControls = 0;
                    for (auto it : itorPipeControl) {
                        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
                        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                            postSyncPipeControls++;
                            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
                            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
                            if constexpr (multiTile == 1) {
                                EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                            } else {
                                EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                            }
                        }
                    }
                    EXPECT_EQ(1u, postSyncPipeControls);
                }
            } else {
                uint32_t packets = event->getMaxPacketsCount();
                EXPECT_EQ(0u, packets % commandList->partitionCount);
                packets /= commandList->partitionCount;
                if (extraSignalStoreDataImm == 0) {
                    packets--;
                }

                ASSERT_EQ(packets, static_cast<uint32_t>(itorStoreDataImm.size()));

                for (uint32_t i = 0; i < packets; i++) {
                    auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
                    EXPECT_EQ(gpuAddress, cmd->getAddress());
                    EXPECT_FALSE(cmd->getStoreQword());
                    EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
                    if constexpr (multiTile == 1) {
                        EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                    } else {
                        EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                    }
                    gpuAddress += (event->getSinglePacketSize() * commandList->partitionCount);
                }

                if (extraSignalStoreDataImm == 0) {
                    uint32_t postSyncPipeControls = 0;
                    for (auto it : itorPipeControl) {
                        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
                        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                            postSyncPipeControls++;
                            EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
                            EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
                            if constexpr (multiTile == 1) {
                                EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                            } else {
                                EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                            }
                        }
                    }
                    EXPECT_EQ(1u, postSyncPipeControls);
                }
            }
        }
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendSignalEventForProfiling() {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
        using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

        bool dynamicAllocSize = (ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset() != ImplicitScalingDispatch<FamilyType>::getTimeStampPostSyncOffset());

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto engineType = copyOnly == 1 ? NEO::EngineGroupType::copy : NEO::EngineGroupType::compute;
        auto result = commandList->initialize(device, engineType, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto cmdStream = commandList->commandContainer.getCommandStream();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
        ASSERT_NE(nullptr, event.get());

        commandList->setupTimestampEventForMultiTile(event.get());
        size_t sizeBefore = cmdStream->getUsed();
        commandList->appendEventForProfiling(event.get(), nullptr, false, false, false, false);
        size_t sizeAfter = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), sizeBefore),
            (sizeAfter - sizeBefore)));

        if (dynamicAllocSize && commandList->partitionCount > 1) {
            auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*cmdList.begin());
            ASSERT_NE(nullptr, lriCmd);

            EXPECT_EQ(NEO::PartitionRegisters<FamilyType>::addressOffsetCCSOffset, lriCmd->getRegisterOffset());
            EXPECT_EQ(NEO::ImplicitScalingDispatch<FamilyType>::getTimeStampPostSyncOffset(), lriCmd->getDataDword());
        }

        auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());

        if constexpr (limitEventPacketes == 1) {
            constexpr uint32_t expectedStoreDataImm = 0;
            ASSERT_EQ(expectedStoreDataImm, itorStoreDataImm.size());
        } else {
            uint32_t packetUsed = event->getPacketsInUse();
            uint32_t remainingPackets = event->getMaxPacketsCount() - packetUsed;
            remainingPackets /= commandList->partitionCount;
            ASSERT_EQ(remainingPackets, static_cast<uint32_t>(itorStoreDataImm.size()));

            uint64_t gpuAddress = event->getCompletionFieldGpuAddress(device);
            gpuAddress += (packetUsed * event->getSinglePacketSize());

            for (uint32_t i = 0; i < remainingPackets; i++) {
                auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
                EXPECT_EQ(gpuAddress, cmd->getAddress());
                EXPECT_FALSE(cmd->getStoreQword());
                EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
                if constexpr (multiTile == 1) {
                    EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                } else {
                    EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                }
                gpuAddress += (event->getSinglePacketSize() * commandList->partitionCount);
            }
        }

        if (dynamicAllocSize && commandList->partitionCount > 1) {
            auto lriCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(*cmdList.rbegin());
            ASSERT_NE(nullptr, lriCmd);

            EXPECT_EQ(NEO::PartitionRegisters<FamilyType>::addressOffsetCCSOffset, lriCmd->getRegisterOffset());
            EXPECT_EQ(NEO::ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset(), lriCmd->getDataDword());
        }
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendSignalEventPostAppendCall(ze_event_pool_flags_t eventPoolFlags) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
        using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto engineType = copyOnly == 1 ? NEO::EngineGroupType::copy : NEO::EngineGroupType::compute;
        auto result = commandList->initialize(device, engineType, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto cmdStream = commandList->commandContainer.getCommandStream();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
        ASSERT_NE(nullptr, event.get());

        commandList->setupTimestampEventForMultiTile(event.get());
        size_t sizeBefore = cmdStream->getUsed();
        commandList->appendSignalEventPostWalker(event.get(), nullptr, nullptr, false, false, copyOnly);
        size_t sizeAfter = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), sizeBefore),
            (sizeAfter - sizeBefore)));

        uint64_t gpuAddress = event->getCompletionFieldGpuAddress(device);

        if constexpr (copyOnly == 1) {
            auto itorFlushDw = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

            uint32_t flushCmdWaFactor = 1;
            NEO::EncodeDummyBlitWaArgs waArgs{false, &(device->getNEODevice()->getRootDeviceEnvironmentRef())};
            if (MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs) > 0) {
                flushCmdWaFactor++;
            }

            uint32_t expectedFlushDw = event->getMaxPacketsCount();
            expectedFlushDw *= flushCmdWaFactor;
            ASSERT_EQ(expectedFlushDw, itorFlushDw.size());

            uint32_t startingSignalCmd = 0;
            if (eventPoolFlags != 0) {
                auto cmd = genCmdCast<MI_FLUSH_DW *>(*itorFlushDw[(flushCmdWaFactor - 1)]);
                EXPECT_EQ(0u, cmd->getDestinationAddress());
                EXPECT_EQ(0u, cmd->getImmediateData());

                startingSignalCmd = flushCmdWaFactor;
                gpuAddress += event->getSinglePacketSize();
            }

            for (uint32_t i = startingSignalCmd; i < expectedFlushDw; i++) {
                auto cmd = genCmdCast<MI_FLUSH_DW *>(*itorFlushDw[i]);
                if (flushCmdWaFactor == 2) {
                    // even flush commands are WAs
                    if ((i & 1) == 0) {
                        continue;
                    }
                }
                EXPECT_EQ(gpuAddress, cmd->getDestinationAddress());
                EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
                gpuAddress += event->getSinglePacketSize();
            }

        } else {
            auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
            auto itorPipeControl = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

            uint32_t expectedPostSyncPipeControls = 0;
            if (eventPoolFlags == 0) {
                expectedPostSyncPipeControls = 1;
            }

            if constexpr (limitEventPacketes == 1) {
                constexpr uint32_t expectedStoreDataImm = 0;
                ASSERT_EQ(expectedStoreDataImm, itorStoreDataImm.size());

                uint32_t postSyncPipeControls = 0;
                for (auto it : itorPipeControl) {
                    auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
                    if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                        postSyncPipeControls++;
                        EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
                        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
                        if constexpr (multiTile == 1) {
                            EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                        } else {
                            EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                        }
                    }
                }
                EXPECT_EQ(expectedPostSyncPipeControls, postSyncPipeControls);

            } else {
                uint32_t packets = event->getMaxPacketsCount();
                EXPECT_EQ(0u, packets % commandList->partitionCount);
                packets /= commandList->partitionCount;
                packets--;

                ASSERT_EQ(packets, static_cast<uint32_t>(itorStoreDataImm.size()));

                if (eventPoolFlags != 0) {
                    gpuAddress += (event->getSinglePacketSize() * commandList->partitionCount);
                }

                for (uint32_t i = 0; i < packets; i++) {
                    auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
                    EXPECT_EQ(gpuAddress, cmd->getAddress());
                    EXPECT_FALSE(cmd->getStoreQword());
                    EXPECT_EQ(Event::STATE_SIGNALED, cmd->getDataDword0());
                    if constexpr (multiTile == 1) {
                        EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                    } else {
                        EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                    }
                    gpuAddress += (event->getSinglePacketSize() * commandList->partitionCount);
                }

                uint32_t postSyncPipeControls = 0;
                for (auto it : itorPipeControl) {
                    auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
                    if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                        postSyncPipeControls++;
                        EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
                        EXPECT_EQ(Event::STATE_SIGNALED, cmd->getImmediateData());
                        if constexpr (multiTile == 1) {
                            EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                        } else {
                            EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                        }
                    }
                }
                EXPECT_EQ(expectedPostSyncPipeControls, postSyncPipeControls);
            }
        }
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendWaitEvent(ze_event_pool_flags_t eventPoolFlags) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
        using COMPARE_OPERATION = typename FamilyType::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto cmdStream = commandList->commandContainer.getCommandStream();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
        ASSERT_NE(nullptr, event.get());

        size_t sizeBefore = cmdStream->getUsed();
        auto eventHandle = event->toHandle();
        result = commandList->appendWaitOnEvents(1, &eventHandle, nullptr, false, true, false, false, false, false);
        size_t sizeAfter = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), sizeBefore),
            (sizeAfter - sizeBefore)));

        auto itorSemWait = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());

        if constexpr (limitEventPacketes == 1 && multiTile == 0) {
            ASSERT_EQ(1u, itorSemWait.size());
        } else {
            uint32_t allPackets = event->getMaxPacketsCount();
            ASSERT_EQ(allPackets, static_cast<uint32_t>(itorSemWait.size()));

            uint64_t gpuAddress = event->getCompletionFieldGpuAddress(device);

            for (uint32_t i = 0; i < allPackets; i++) {
                auto cmd = genCmdCast<MI_SEMAPHORE_WAIT *>(*itorSemWait[i]);
                EXPECT_EQ(gpuAddress, cmd->getSemaphoreGraphicsAddress());
                EXPECT_EQ(COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, cmd->getCompareOperation());
                EXPECT_EQ(Event::STATE_CLEARED, cmd->getSemaphoreDataDword());
                gpuAddress += event->getSinglePacketSize();
            }
        }
    }

    template <GFXCORE_FAMILY gfxCoreFamily>
    void testAppendResetEvent(ze_event_pool_flags_t eventPoolFlags) {
        using FamilyType = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
        using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
        using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
        using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
        using POST_SYNC_OPERATION = typename FamilyType::PIPE_CONTROL::POST_SYNC_OPERATION;

        auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
        auto engineType = copyOnly == 1 ? NEO::EngineGroupType::copy : NEO::EngineGroupType::compute;
        auto result = commandList->initialize(device, engineType, 0u);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto cmdStream = commandList->commandContainer.getCommandStream();

        ze_event_pool_desc_t eventPoolDesc = {};
        eventPoolDesc.count = 1;
        eventPoolDesc.flags = eventPoolFlags;

        ze_event_desc_t eventDesc = {};
        eventDesc.index = 0;
        eventDesc.signal = 0;

        auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
        ASSERT_NE(nullptr, event.get());

        if (this->alignEventPacketsForReset) {
            event->setPacketsInUse(commandList->partitionCount);
        }

        size_t sizeBefore = cmdStream->getUsed();
        result = commandList->appendEventReset(event->toHandle());
        size_t sizeAfter = cmdStream->getUsed();
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        uint64_t gpuAddress = event->getCompletionFieldGpuAddress(device);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList,
            ptrOffset(cmdStream->getCpuBase(), sizeBefore),
            (sizeAfter - sizeBefore)));

        if constexpr (copyOnly == 1) {
            uint32_t flushCmdWaFactor = 1;
            NEO::EncodeDummyBlitWaArgs waArgs{false, &(device->getNEODevice()->getRootDeviceEnvironmentRef())};
            if (MockEncodeMiFlushDW<FamilyType>::getWaSize(waArgs) > 0) {
                flushCmdWaFactor++;
            }

            auto itorFlushDw = findAll<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

            uint32_t expectedFlushDw = event->getMaxPacketsCount() * flushCmdWaFactor;
            ASSERT_EQ(expectedFlushDw, itorFlushDw.size());

            for (uint32_t i = 0; i < expectedFlushDw; i++) {
                auto cmd = genCmdCast<MI_FLUSH_DW *>(*itorFlushDw[i]);
                if (flushCmdWaFactor == 2) {
                    // even flush commands are WAs
                    if ((i & 1) == 0) {
                        continue;
                    }
                }
                EXPECT_EQ(gpuAddress, cmd->getDestinationAddress());
                EXPECT_EQ(Event::STATE_CLEARED, cmd->getImmediateData());
                gpuAddress += event->getSinglePacketSize();
            }

        } else {
            auto itorStoreDataImm = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
            auto itorPipeControl = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

            uint32_t extraCleanupStoreDataImm = 0;
            if constexpr (multiTile == 1) {
                // multi-tile barrier self-cleanup
                extraCleanupStoreDataImm = 2;
            }

            uint32_t expectedStoreDataImm = event->getMaxPacketsCount() / commandList->partitionCount;
            if constexpr (limitEventPacketes == 1) {
                // single packet will be reset by PC or SDI
                expectedStoreDataImm = 1;
            }

            uint32_t expectedPostSyncPipeControls = 0;
            // last packet is reset by PIPE_CONTROL w/ post sync
            if (eventPoolFlags == ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP) {
                expectedStoreDataImm--;
                expectedPostSyncPipeControls = 1;
            }

            ASSERT_EQ(expectedStoreDataImm + extraCleanupStoreDataImm, static_cast<uint32_t>(itorStoreDataImm.size()));

            for (uint32_t i = 0; i < expectedStoreDataImm; i++) {
                auto cmd = genCmdCast<MI_STORE_DATA_IMM *>(*itorStoreDataImm[i]);
                EXPECT_EQ(gpuAddress, cmd->getAddress());
                EXPECT_FALSE(cmd->getStoreQword());
                EXPECT_EQ(Event::STATE_CLEARED, cmd->getDataDword0());
                if constexpr (multiTile == 1) {
                    EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                } else {
                    EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                }

                gpuAddress += event->getSinglePacketSize() * commandList->partitionCount;
            }
            uint32_t postSyncPipeControls = 0;
            for (auto it : itorPipeControl) {
                auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
                if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
                    postSyncPipeControls++;
                    EXPECT_EQ(gpuAddress, NEO::UnitTestHelper<FamilyType>::getPipeControlPostSyncAddress(*cmd));
                    EXPECT_EQ(Event::STATE_CLEARED, cmd->getImmediateData());
                    if constexpr (multiTile == 1) {
                        EXPECT_TRUE(cmd->getWorkloadPartitionIdOffsetEnable());
                    } else {
                        EXPECT_FALSE(cmd->getWorkloadPartitionIdOffsetEnable());
                    }
                }
            }
            EXPECT_EQ(expectedPostSyncPipeControls, postSyncPipeControls);
        }
    }

    DebugManagerStateRestore restorer;

    Mock<::L0::KernelImp> kernel;
    std::unique_ptr<Mock<Module>> module;
    bool alignEventPacketsForReset = true;
};

using CommandListSignalAllEventPacketTest = Test<CommandListSignalAllEventPacketFixture<0, 0, 0>>;
HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendKernelThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendKernel<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendKernelThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendKernel<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendWaitEventThenAllPacketWaitDispatched, IsAtLeastXeCore) {
    testAppendWaitEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendWaitEventThenAllPacketWaitDispatched, IsAtLeastXeCore) {
    testAppendWaitEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalProfilingEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEventForProfiling<FamilyType::gfxCoreFamily>();
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(0);
}

using MultiTileCommandListSignalAllEventPacketTest = Test<CommandListSignalAllEventPacketFixture<1, 0, 0>>;
HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendKernelThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendKernel<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendKernelThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendKernel<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendWaitEventThenAllPacketWaitDispatched, IsAtLeastXeCore) {
    testAppendWaitEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendWaitEventThenAllPacketWaitDispatched, IsAtLeastXeCore) {
    testAppendWaitEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalProfilingEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEventForProfiling<FamilyType::gfxCoreFamily>();
}

struct MultiTileCommandListSignalAllocLayoutTest : public MultiTileCommandListSignalAllEventPacketTest {
    void SetUp() override {
        MultiTileCommandListSignalAllEventPacketTest::SetUp();
    }
};

HWTEST2_F(MultiTileCommandListSignalAllocLayoutTest, givenDynamicLayoutEnabledWhenAppendEventForProfilingCalledThenProgramOffsetMmio, IsAtLeastXeCore) {
    if constexpr (FamilyType::isHeaplessRequired() == false) {
        EXPECT_NE(ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset(), ImplicitScalingDispatch<FamilyType>::getTimeStampPostSyncOffset());
    } else {
        EXPECT_EQ(ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset(), ImplicitScalingDispatch<FamilyType>::getTimeStampPostSyncOffset());
    }

    testAppendSignalEventForProfiling<FamilyType::gfxCoreFamily>();
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventWithSinglePacketThenAllPacketResetDispatchedUsingNonPartitionedWrite, IsAtLeastXeCore) {
    alignEventPacketsForReset = false;

    testAppendResetEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventWithSinglePacketThenAllPacketResetDispatchedUsingNonPartitionedWrite, IsAtLeastXeCore) {
    alignEventPacketsForReset = false;

    testAppendResetEvent<FamilyType::gfxCoreFamily>(0);
}

using CommandListSignalAllEventPacketForCompactEventTest = Test<CommandListSignalAllEventPacketFixture<0, 1, 0>>;
HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendKernelThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendKernel<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendKernelThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendKernel<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendWaitEventThenAllPacketWaitDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendWaitEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendWaitEventThenAllPacketWaitDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendWaitEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalProfilingEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEventForProfiling<FamilyType::gfxCoreFamily>();
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(0);
}

using MultiTileCommandListSignalAllEventPacketForCompactEventTest = Test<CommandListSignalAllEventPacketFixture<1, 1, 0>>;
HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendKernelThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendKernel<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendKernelThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendKernel<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendWaitEventThenAllPacketWaitDispatchedAsDefaultActiveSinglePacket, IsAtLeastXeCore) {
    testAppendWaitEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendWaitEventThenAllPacketWaitDispatchedAsDefaultActiveSinglePacket, IsAtLeastXeCore) {
    testAppendWaitEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalProfilingEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEventForProfiling<FamilyType::gfxCoreFamily>();
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventWithSinglePacketThenAllPacketResetDispatchedUsingNonPartitionedWrite, IsAtLeastXeCore) {
    alignEventPacketsForReset = false;

    testAppendResetEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventWithSinglePacketThenAllPacketResetDispatchedUsingNonPartitionedWrite, IsAtLeastXeCore) {
    alignEventPacketsForReset = false;

    testAppendResetEvent<FamilyType::gfxCoreFamily>(0);
}

using CopyCommandListSignalAllEventPacketTest = Test<CommandListSignalAllEventPacketFixture<0, 0, 1>>;
HWTEST2_F(CopyCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(0);
}

using MultiTileCopyCommandListSignalAllEventPacketTest = Test<CommandListSignalAllEventPacketFixture<1, 0, 1>>;
HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatched, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(0);
}

using CopyCommandListSignalAllEventPacketForCompactEventTest = Test<CommandListSignalAllEventPacketFixture<0, 1, 1>>;
HWTEST2_F(CopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(0);
}

using MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest = Test<CommandListSignalAllEventPacketFixture<1, 1, 1>>;
HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEvent<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendSignalEventPostAppendCall<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeCore) {
    testAppendResetEvent<FamilyType::gfxCoreFamily>(0);
}

using RayTracingMatcher = IsAtLeastXeCore;

using CommandListAppendLaunchRayTracingKernelTest = Test<CommandListAppendLaunchRayTracingKernelFixture>;

HWTEST2_F(CommandListAppendLaunchRayTracingKernelTest, givenKernelUsingRayTracingWhenAppendLaunchKernelIsCalledThenSuccessIsReturned, RayTracingMatcher) {
    VariableBackup<GraphicsAllocation *> rtMemoryBackedBuffer{&neoDevice->rtMemoryBackedBuffer};

    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    kernel.immutableData.kernelDescriptor->kernelAttributes.flags.hasRTCalls = true;

    rtMemoryBackedBuffer = nullptr;
    CmdListKernelLaunchParams launchParams = {};
    result = pCommandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    rtMemoryBackedBuffer = buffer1;
    result = pCommandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using RayTracingCmdListTest = Test<RayTracingCmdListFixture>;

template <typename FamilyType>
void findStateCacheFlushPipeControlAfterWalker(LinearStream &cmdStream, size_t offset, size_t size) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream.getCpuBase(), offset),
        size));

    auto walkerIt = NEO::UnitTestHelper<FamilyType>::findWalkerTypeCmd(cmdList.begin(), cmdList.end());

    ASSERT_NE(cmdList.end(), walkerIt);

    auto pcItorList = findAll<PIPE_CONTROL *>(walkerIt, cmdList.end());

    bool stateCacheFlushFound = false;
    for (auto &cmdIt : pcItorList) {
        auto pipeControl = reinterpret_cast<PIPE_CONTROL *>(*cmdIt);

        if (pipeControl->getStateCacheInvalidationEnable()) {
            stateCacheFlushFound = true;
            break;
        }
    }

    EXPECT_TRUE(stateCacheFlushFound);
}

template <typename FamilyType>
void find3dBtdCommand(LinearStream &cmdStream, size_t offset, size_t size, uint64_t gpuVa, bool expectToFind) {
    using _3DSTATE_BTD = typename FamilyType::_3DSTATE_BTD;

    if (expectToFind) {
        ASSERT_NE(0u, size);
    } else if (size == 0) {
        return;
    }

    bool btdCommandFound = false;
    size_t btdStateCmdCount = 0;
    if (expectToFind) {
        btdStateCmdCount = 1;
    }

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream.getCpuBase(), offset),
        size));

    auto btdStateCmdList = findAll<_3DSTATE_BTD *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(btdStateCmdCount, btdStateCmdList.size());

    if (btdStateCmdCount > 0) {
        auto btdStateCmd = reinterpret_cast<_3DSTATE_BTD *>(*btdStateCmdList[0]);
        EXPECT_EQ(gpuVa, btdStateCmd->getMemoryBackedBufferBasePointer());

        btdCommandFound = true;
    }

    EXPECT_EQ(expectToFind, btdCommandFound);
}

HWTEST2_F(RayTracingCmdListTest,
          givenRayTracingKernelWhenRegularCmdListExecutedAndRegularExecutedAgainThenDispatch3dBtdCommandOnceMakeResidentTwiceAndPipeControlWithStateCacheFlushAfterWalker,
          RayTracingMatcher) {
    if (device->getNEODevice()->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->storeMakeResidentAllocations = true;

    auto &containerRegular = commandList->getCmdContainer();
    auto &cmdStreamRegular = *containerRegular.getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t regularSizeBefore = cmdStreamRegular.getUsed();
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t regularSizeAfter = cmdStreamRegular.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    findStateCacheFlushPipeControlAfterWalker<FamilyType>(cmdStreamRegular, regularSizeBefore, regularSizeAfter - regularSizeBefore);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueSizeBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueSizeAfter = cmdQueueStream.getUsed();

    // find 3D BTD command - first time use
    find3dBtdCommand<FamilyType>(cmdQueueStream, queueSizeBefore, queueSizeAfter - queueSizeBefore, rtAllocationAddress, true);

    uint32_t residentCount = 1;
    ultCsr->isMadeResident(rtAllocation, residentCount);

    queueSizeBefore = cmdQueueStream.getUsed();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    queueSizeAfter = cmdQueueStream.getUsed();

    // no find - subsequent dispatch
    find3dBtdCommand<FamilyType>(cmdQueueStream, queueSizeBefore, queueSizeAfter - queueSizeBefore, rtAllocationAddress, false);

    residentCount++;
    ultCsr->isMadeResident(rtAllocation, residentCount);
}

HWTEST2_F(RayTracingCmdListTest,
          givenRayTracingKernelWhenRegularCmdListExecutedAndImmediateExecutedAgainThenDispatch3dBtdCommandOnceMakeResidentTwiceAndPipeControlWithStateCacheFlushAfterWalker,
          RayTracingMatcher) {
    if (device->getNEODevice()->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->storeMakeResidentAllocations = true;

    auto &containerRegular = commandList->getCmdContainer();
    auto &cmdStreamRegular = *containerRegular.getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t regularSizeBefore = cmdStreamRegular.getUsed();
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t regularSizeAfter = cmdStreamRegular.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    findStateCacheFlushPipeControlAfterWalker<FamilyType>(cmdStreamRegular, regularSizeBefore, regularSizeAfter - regularSizeBefore);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueSizeBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueSizeAfter = cmdQueueStream.getUsed();

    find3dBtdCommand<FamilyType>(cmdQueueStream, queueSizeBefore, queueSizeAfter - queueSizeBefore, rtAllocationAddress, true);

    uint32_t residentCount = 1;
    ultCsr->isMadeResident(rtAllocation, residentCount);

    auto &containerImmediate = commandListImmediate->getCmdContainer();
    auto &cmdStreamImmediate = *containerImmediate.getCommandStream();

    auto &csrStream = ultCsr->commandStream;

    size_t immediateSizeBefore = cmdStreamImmediate.getUsed();
    size_t csrBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrAfter = csrStream.getUsed();
    size_t immediateSizeAfter = cmdStreamImmediate.getUsed();

    findStateCacheFlushPipeControlAfterWalker<FamilyType>(cmdStreamImmediate, immediateSizeBefore, immediateSizeAfter - immediateSizeBefore);

    find3dBtdCommand<FamilyType>(csrStream, csrBefore, csrAfter - csrBefore, rtAllocationAddress, false);

    residentCount++;
    ultCsr->isMadeResident(rtAllocation, residentCount);
}

HWTEST2_F(RayTracingCmdListTest,
          givenRayTracingKernelWhenImmediateCmdListExecutedAndImmediateExecutedAgainThenDispatch3dBtdCommandOnceMakeResidentTwiceAndPipeControlWithStateCacheFlushAfterWalker,
          RayTracingMatcher) {
    if (device->getNEODevice()->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->storeMakeResidentAllocations = true;

    auto &containerImmediate = commandListImmediate->getCmdContainer();
    auto &cmdStreamImmediate = *containerImmediate.getCommandStream();

    auto &csrStream = ultCsr->commandStream;

    size_t immediateSizeBefore = cmdStreamImmediate.getUsed();
    size_t csrBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrAfter = csrStream.getUsed();
    size_t immediateSizeAfter = cmdStreamImmediate.getUsed();

    findStateCacheFlushPipeControlAfterWalker<FamilyType>(cmdStreamImmediate, immediateSizeBefore, immediateSizeAfter - immediateSizeBefore);

    find3dBtdCommand<FamilyType>(csrStream, csrBefore, csrAfter - csrBefore, rtAllocationAddress, true);

    uint32_t residentCount = 1;
    ultCsr->isMadeResident(rtAllocation, residentCount);

    immediateSizeBefore = cmdStreamImmediate.getUsed();
    csrBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    csrAfter = csrStream.getUsed();
    immediateSizeAfter = cmdStreamImmediate.getUsed();

    findStateCacheFlushPipeControlAfterWalker<FamilyType>(cmdStreamImmediate, immediateSizeBefore, immediateSizeAfter - immediateSizeBefore);

    find3dBtdCommand<FamilyType>(csrStream, csrBefore, csrAfter - csrBefore, rtAllocationAddress, false);

    residentCount++;
    ultCsr->isMadeResident(rtAllocation, residentCount);
}

HWTEST2_F(RayTracingCmdListTest,
          givenRayTracingKernelWhenImmediateCmdListExecutedAndRegularExecutedAgainThenDispatch3dBtdCommandOnceMakeResidentTwiceAndPipeControlWithStateCacheFlushAfterWalker,
          RayTracingMatcher) {
    if (device->getNEODevice()->getCompilerProductHelper().isHeaplessModeEnabled(*defaultHwInfo)) {
        GTEST_SKIP();
    }

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->storeMakeResidentAllocations = true;

    auto &containerImmediate = commandListImmediate->getCmdContainer();
    auto &cmdStreamImmediate = *containerImmediate.getCommandStream();

    auto &csrStream = ultCsr->commandStream;

    size_t immediateSizeBefore = cmdStreamImmediate.getUsed();
    size_t csrBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrAfter = csrStream.getUsed();
    size_t immediateSizeAfter = cmdStreamImmediate.getUsed();

    findStateCacheFlushPipeControlAfterWalker<FamilyType>(cmdStreamImmediate, immediateSizeBefore, immediateSizeAfter - immediateSizeBefore);

    find3dBtdCommand<FamilyType>(csrStream, csrBefore, csrAfter - csrBefore, rtAllocationAddress, true);

    uint32_t residentCount = 1;
    ultCsr->isMadeResident(rtAllocation, residentCount);

    auto &containerRegular = commandList->getCmdContainer();
    auto &cmdStreamRegular = *containerRegular.getCommandStream();

    size_t regularSizeBefore = cmdStreamRegular.getUsed();
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t regularSizeAfter = cmdStreamRegular.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    findStateCacheFlushPipeControlAfterWalker<FamilyType>(cmdStreamRegular, regularSizeBefore, regularSizeAfter - regularSizeBefore);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueSizeBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueSizeAfter = cmdQueueStream.getUsed();

    find3dBtdCommand<FamilyType>(cmdQueueStream, queueSizeBefore, queueSizeAfter - queueSizeBefore, rtAllocationAddress, false);

    residentCount++;
    ultCsr->isMadeResident(rtAllocation, residentCount);
}

using ImmediateFlushTaskGlobalStatelessCmdListTest = Test<ImmediateFlushTaskGlobalStatelessCmdListFixture>;

HWTEST2_F(ImmediateFlushTaskGlobalStatelessCmdListTest,
          givenImmediateFlushOnGlobalStatelessWhenAppendingKernelThenExpectStateBaseAddressCommandDispatchedOnce,
          IsAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csrImmediate.storeMakeResidentAllocations = true;
    if (csrImmediate.heaplessStateInitialized) {
        GTEST_SKIP();
    }
    auto &csrStream = csrImmediate.commandStream;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t csrUsedBefore = csrStream.getUsed();
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto globalSurfaceHeap = commandListImmediate->getCsr(false)->getGlobalStatelessHeap();

    auto ioHeap = commandListImmediate->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject);
    auto ioBaseAddress = neoDevice->getGmmHelper()->decanonize(ioHeap->getHeapGpuBase());

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());
    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_EQ(ioBaseAddress, sbaCmd->getGeneralStateBaseAddress());

    EXPECT_TRUE(csrImmediate.isMadeResident(ioHeap->getGraphicsAllocation()));
    EXPECT_TRUE(csrImmediate.isMadeResident(globalSurfaceHeap->getGraphicsAllocation()));

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, sbaCmds.size());
}

HWTEST2_F(ImmediateFlushTaskGlobalStatelessCmdListTest,
          givenImmediateFlushOnGlobalStatelessWhenAppendingSecondKernelWithChangedMocsThenExpectStateBaseAddressCommandDispatchedTwiceWithChangedMocs,
          IsAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    if (neoDevice->getProductHelper().isNewCoherencyModelSupported()) {
        GTEST_SKIP();
    }

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t csrUsedBefore = csrStream.getUsed();
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto globalSurfaceHeap = commandListImmediate->getCsr(false)->getGlobalStatelessHeap();

    auto ssBaseAddress = globalSurfaceHeap->getHeapGpuBase();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());
    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    uint32_t cachedStatlessMocs = getMocs(true);
    EXPECT_EQ((cachedStatlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    kernel->privateState.kernelRequiresUncachedMocsCount++;

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());
    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    uint32_t uncachedStatlessMocs = getMocs(false);
    EXPECT_EQ((uncachedStatlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

using ImmediateFlushTaskCsrSharedHeapCmdListTest = Test<ImmediateFlushTaskCsrSharedHeapCmdListFixture>;

HWTEST2_F(ImmediateFlushTaskCsrSharedHeapCmdListTest,
          givenImmediateFlushOnCsrSharedHeapsWhenAppendingKernelThenExpectStateBaseAddressCommandDispatchedOnce,
          IsAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto bindlessHeapsHelper = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.get();
    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csrImmediate.storeMakeResidentAllocations = true;
    if (csrImmediate.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    auto &csrStream = csrImmediate.commandStream;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t csrUsedBefore = csrStream.getUsed();
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto ssHeap = commandListImmediate->getCmdContainer().getSurfaceStateHeapReserve().indirectHeapReservation;
    auto ssBaseAddress = ssHeap->getHeapGpuBase();

    uint64_t dsBaseAddress = 0;

    if (dshRequired) {
        if (bindlessHeapsHelper) {
            dsBaseAddress = bindlessHeapsHelper->getGlobalHeapsBase();
        } else {
            auto dsHeap = commandListImmediate->getCmdContainer().getDynamicStateHeapReserve().indirectHeapReservation;
            dsBaseAddress = dsHeap->getHeapGpuBase();
        }
    }

    auto ioHeap = commandListImmediate->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject);
    auto ioBaseAddress = neoDevice->getGmmHelper()->decanonize(ioHeap->getHeapGpuBase());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());
    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_EQ(dshRequired, sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_EQ(dsBaseAddress, sbaCmd->getDynamicStateBaseAddress());

    EXPECT_EQ(ioBaseAddress, sbaCmd->getGeneralStateBaseAddress());

    EXPECT_TRUE(csrImmediate.isMadeResident(ioHeap->getGraphicsAllocation()));
    EXPECT_TRUE(csrImmediate.isMadeResident(ssHeap->getGraphicsAllocation()));
    if (dshRequired) {
        auto dsHeap = commandListImmediate->getCmdContainer().getDynamicStateHeapReserve().indirectHeapReservation;
        EXPECT_TRUE(csrImmediate.isMadeResident(dsHeap->getGraphicsAllocation()));
    }

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, sbaCmds.size());
}

HWTEST2_F(ImmediateFlushTaskCsrSharedHeapCmdListTest,
          givenImmediateFlushOnCsrSharedHeapsWhenAppendingSecondKernelWithChangedMocsThenExpectStateBaseAddressCommandDispatchedTwiceWithChangedMocs,
          IsAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    if (neoDevice->getProductHelper().isNewCoherencyModelSupported()) {
        GTEST_SKIP();
    }

    auto bindlessHeapsHelper = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.get();
    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t csrUsedBefore = csrStream.getUsed();
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto ssHeap = commandListImmediate->getCmdContainer().getSurfaceStateHeapReserve().indirectHeapReservation;
    auto ssBaseAddress = ssHeap->getHeapGpuBase();

    uint64_t dsBaseAddress = 0;
    if (dshRequired) {
        if (bindlessHeapsHelper) {
            dsBaseAddress = bindlessHeapsHelper->getGlobalHeapsBase();
        } else {
            auto dsHeap = commandListImmediate->getCmdContainer().getDynamicStateHeapReserve().indirectHeapReservation;
            dsBaseAddress = dsHeap->getHeapGpuBase();
        }
    }

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());
    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_EQ(dshRequired, sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_EQ(dsBaseAddress, sbaCmd->getDynamicStateBaseAddress());

    uint32_t cachedStatlessMocs = getMocs(true);
    EXPECT_EQ((cachedStatlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());

    kernel->privateState.kernelRequiresUncachedMocsCount++;

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());
    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    uint32_t uncachedStatlessMocs = getMocs(false);
    EXPECT_EQ((uncachedStatlessMocs << 1), sbaCmd->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(ImmediateFlushTaskCsrSharedHeapCmdListTest,
          givenImmediateFlushOnCsrSharedHeapsWhenAppendingSecondKernelWithScratchThenExpectScratchStateAndAllocation,
          IsHeapfulRequiredAndAtLeastXeCore) {
    using CFE_STATE = typename FamilyType::CFE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    if (csrImmediate.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    csrImmediate.storeMakeResidentAllocations = true;
    auto &csrStream = csrImmediate.commandStream;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t csrUsedBefore = csrStream.getUsed();
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto frontEndCommands = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, frontEndCommands.size());
    auto frontEndCmd = reinterpret_cast<CFE_STATE *>(*frontEndCommands[0]);

    EXPECT_EQ(0u, frontEndCmd->getScratchSpaceBuffer());

    EXPECT_EQ(nullptr, csrImmediate.getScratchSpaceController()->getScratchSpaceSlot0Allocation());

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x100;

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    frontEndCommands = findAll<CFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, frontEndCommands.size());
    frontEndCmd = reinterpret_cast<CFE_STATE *>(*frontEndCommands[0]);

    constexpr size_t expectedScratchOffset = 2 * sizeof(RENDER_SURFACE_STATE);
    EXPECT_EQ(expectedScratchOffset, frontEndCmd->getScratchSpaceBuffer());

    auto scratchAllocation = csrImmediate.getScratchSpaceController()->getScratchSpaceSlot0Allocation();
    ASSERT_NE(nullptr, scratchAllocation);

    EXPECT_TRUE(csrImmediate.isMadeResident(scratchAllocation));

    auto ssHeap = commandListImmediate->getCmdContainer().getSurfaceStateHeapReserve().indirectHeapReservation;
    void *scratchSurfaceStateMemory = ptrOffset(ssHeap->getCpuBase(), expectedScratchOffset);

    auto scratchSurfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(scratchSurfaceStateMemory);
    EXPECT_EQ(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_SCRATCH, scratchSurfaceState->getSurfaceType());
    EXPECT_EQ(scratchAllocation->getGpuAddress(), scratchSurfaceState->getSurfaceBaseAddress());
}

HWTEST2_F(ImmediateFlushTaskCsrSharedHeapCmdListTest,
          givenImmediateFlushOnCsrSharedHeapsWhenAppendingBarrierThenNoSurfaceHeapAllocated,
          IsAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;
    if (csrImmediate.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    size_t csrUsedBefore = csrStream.getUsed();
    auto result = commandListImmediate->appendBarrier(nullptr, 0, nullptr, false);
    size_t csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto ssHeap = commandListImmediate->getCmdContainer().getSurfaceStateHeapReserve().indirectHeapReservation;
    EXPECT_EQ(nullptr, ssHeap->getGraphicsAllocation());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());
    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_FALSE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(0u, sbaCmd->getSurfaceStateBaseAddress());
}

HWTEST2_F(ImmediateFlushTaskCsrSharedHeapCmdListTest,
          givenImmediateFlushOnCsrSharedHeapsFirstWhenExecuteRegularCommandListSecondAndImmediateThirdThenExpectSharedHeapBaseAddressRestored,
          IsAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto bindlessHeapsHelper = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.get();
    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    if (csrImmediate.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    auto &csrStream = csrImmediate.commandStream;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t csrUsedBefore = csrStream.getUsed();
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto ssSharedHeap = commandListImmediate->getCmdContainer().getSurfaceStateHeapReserve().indirectHeapReservation;
    EXPECT_NE(nullptr, ssSharedHeap->getGraphicsAllocation());
    auto ssShareBaseAddress = ssSharedHeap->getHeapGpuBase();

    auto dsSharedHeap = commandListImmediate->getCmdContainer().getDynamicStateHeapReserve().indirectHeapReservation;
    uint64_t dsShareBaseAddress = 0;
    if (this->dshRequired) {
        EXPECT_NE(nullptr, dsSharedHeap->getGraphicsAllocation());
        if (bindlessHeapsHelper) {
            dsShareBaseAddress = bindlessHeapsHelper->getGlobalHeapsBase();
        } else {
            dsShareBaseAddress = dsSharedHeap->getHeapGpuBase();
        }
    } else {
        EXPECT_EQ(nullptr, dsSharedHeap->getGraphicsAllocation());
    }

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());
    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssShareBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_EQ(this->dshRequired, sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_EQ(dsShareBaseAddress, sbaCmd->getDynamicStateBaseAddress());

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &container = commandList->getCmdContainer();
    auto sshRegularHeap = container.getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssRegularBaseAddress = sshRegularHeap->getHeapGpuBase();

    uint64_t dsRegularBaseAddress = static_cast<uint64_t>(-1);
    if (this->dshRequired) {
        auto dshRegularHeap = container.getIndirectHeap(NEO::HeapType::dynamicState);
        dsRegularBaseAddress = NEO::getStateBaseAddress(*dshRegularHeap, bindlessHeapsHelper);
    }

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &csrBaseAddressState = csrImmediate.getStreamProperties().stateBaseAddress;
    EXPECT_EQ(ssRegularBaseAddress, static_cast<uint64_t>(csrBaseAddressState.surfaceStateBaseAddress.value));
    EXPECT_EQ(dsRegularBaseAddress, static_cast<uint64_t>(csrBaseAddressState.dynamicStateBaseAddress.value));

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());
    sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssShareBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_EQ(this->dshRequired, sbaCmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_EQ(dsShareBaseAddress, sbaCmd->getDynamicStateBaseAddress());
}

using ImmediateFlushTaskPrivateHeapCmdListTest = Test<ImmediateFlushTaskPrivateHeapCmdListFixture>;

HWTEST2_F(ImmediateFlushTaskPrivateHeapCmdListTest,
          givenImmediateFlushOnPrivateHeapsWhenAppendingKernelThenExpectStateBaseAddressCommandDispatchedOnce,
          IsAtLeastXeCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto bindlessHeapsHelper = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[neoDevice->getRootDeviceIndex()]->bindlessHeapsHelper.get();

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    csrImmediate.storeMakeResidentAllocations = true;

    if (csrImmediate.heaplessModeEnabled) {
        GTEST_SKIP();
    }
    auto &csrStream = csrImmediate.commandStream;

    commandListImmediate->getCmdContainer().prepareBindfulSsh();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t csrUsedBefore = csrStream.getUsed();
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    size_t csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto ssHeap = commandListImmediate->getCmdContainer().getIndirectHeap(NEO::HeapType::surfaceState);
    auto ssBaseAddress = ssHeap->getHeapGpuBase();

    uint64_t dsBaseAddress = 0;
    if (dshRequired) {
        auto dsHeap = commandListImmediate->getCmdContainer().getIndirectHeap(NEO::HeapType::dynamicState);
        dsBaseAddress = dsHeap->getHeapGpuBase();
    }

    auto ioHeap = commandListImmediate->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject);
    auto ioBaseAddress = neoDevice->getGmmHelper()->decanonize(ioHeap->getHeapGpuBase());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    auto sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSbaCmds, sbaCmds.size());
    auto sbaCmd = reinterpret_cast<STATE_BASE_ADDRESS *>(*sbaCmds[0]);

    EXPECT_TRUE(sbaCmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssBaseAddress, sbaCmd->getSurfaceStateBaseAddress());

    EXPECT_EQ(dshRequired, sbaCmd->getDynamicStateBaseAddressModifyEnable());
    if (bindlessHeapsHelper) {
        EXPECT_EQ(bindlessHeapsHelper->getGlobalHeapsBase(), sbaCmd->getDynamicStateBaseAddress());
    } else {
        EXPECT_EQ(dsBaseAddress, sbaCmd->getDynamicStateBaseAddress());
    }

    EXPECT_EQ(ioBaseAddress, sbaCmd->getGeneralStateBaseAddress());

    EXPECT_TRUE(csrImmediate.isMadeResident(ioHeap->getGraphicsAllocation()));
    EXPECT_TRUE(csrImmediate.isMadeResident(ssHeap->getGraphicsAllocation()));
    if (dshRequired) {
        auto dsHeap = commandListImmediate->getCmdContainer().getIndirectHeap(NEO::HeapType::dynamicState);
        EXPECT_TRUE(csrImmediate.isMadeResident(dsHeap->getGraphicsAllocation()));
    }

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    csrUsedAfter = csrStream.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(csrStream.getCpuBase(), csrUsedBefore),
        csrUsedAfter - csrUsedBefore));
    sbaCmds = findAll<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_EQ(0u, sbaCmds.size());
}

using CommandListCreate = Test<DeviceFixture>;
HWTEST2_F(CommandListCreate, givenPlatformSupportsHdcUntypedCacheFlushWhenAppendWriteGlobalTimestampThenExpectNoCacheFlushInPostSyncCommand, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::compute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();

    uint64_t timestampAddress = 0x123456785000;
    uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress);

    const auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    returnValue = commandList->appendWriteGlobalTimestamp(dstptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), commandStreamOffset),
        commandContainer.getCommandStream()->getUsed() - commandStreamOffset));

    auto pcList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pcList.size());

    bool timestampPostSyncFound = false;
    for (const auto it : pcList) {
        auto cmd = genCmdCast<PIPE_CONTROL *>(*it);
        if (cmd->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_TIMESTAMP) {
            EXPECT_FALSE(NEO::UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*cmd));
            EXPECT_FALSE(cmd->getUnTypedDataPortCacheFlush());
            timestampPostSyncFound = true;
        }
    }
    EXPECT_TRUE(timestampPostSyncFound);
}

HWTEST2_F(CommandListCreate, givenAppendSignalEventWhenSkipAddToResidencyTrueThenEventAllocationNotAddedToResidency, IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto &commandContainer = commandList->getCmdContainer();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    ASSERT_NE(nullptr, event.get());

    auto &residencyContainer = commandContainer.getResidencyContainer();
    auto eventAllocation = event->getAllocation(device);

    void *pipeControlBuffer = nullptr;

    auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    bool skipAdd = true;
    commandList->appendEventForProfilingAllWalkers(event.get(), &pipeControlBuffer, nullptr, false, true, skipAdd, false);

    auto eventAllocIt = std::find(residencyContainer.begin(), residencyContainer.end(), eventAllocation);
    EXPECT_EQ(residencyContainer.end(), eventAllocIt);

    ASSERT_NE(nullptr, pipeControlBuffer);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), commandStreamOffset),
        commandContainer.getCommandStream()->getUsed() - commandStreamOffset));

    auto pcList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pcList.size());

    PIPE_CONTROL *postSyncPipeControl = nullptr;
    for (const auto it : pcList) {
        postSyncPipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        if (postSyncPipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            break;
        }
    }
    ASSERT_NE(nullptr, postSyncPipeControl);
    ASSERT_EQ(postSyncPipeControl, pipeControlBuffer);

    commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    skipAdd = false;
    commandList->appendEventForProfilingAllWalkers(event.get(), &pipeControlBuffer, nullptr, false, true, skipAdd, false);
    eventAllocIt = std::find(residencyContainer.begin(), residencyContainer.end(), eventAllocation);
    EXPECT_NE(residencyContainer.end(), eventAllocIt);

    cmdList.clear();
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), commandStreamOffset),
        commandContainer.getCommandStream()->getUsed() - commandStreamOffset));

    pcList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pcList.size());

    postSyncPipeControl = nullptr;
    for (const auto it : pcList) {
        postSyncPipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        if (postSyncPipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            break;
        }
    }
    ASSERT_NE(nullptr, postSyncPipeControl);
    ASSERT_EQ(postSyncPipeControl, pipeControlBuffer);
}

HWTEST2_F(CommandListCreate,
          givenAppendTimestampSignalEventWhenSkipAddToResidencyTrueAndOutRegMemListProvidedThenAllocationNotAddedToResidencyAndStoreRegMemCmdsStored,
          IsAtLeastXeCore) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    auto &commandContainer = commandList->getCmdContainer();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    ASSERT_NE(nullptr, event.get());

    auto &residencyContainer = commandContainer.getResidencyContainer();
    auto eventAllocation = event->getAllocation(device);
    auto eventBaseAddress = event->getGpuAddress(device);

    CommandToPatchContainer outStoreRegMemCmdList;

    auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    bool skipAdd = true;

    bool before = true;
    commandList->appendEventForProfilingAllWalkers(event.get(), nullptr, &outStoreRegMemCmdList, before, true, skipAdd, false);
    before = false;
    commandList->appendEventForProfilingAllWalkers(event.get(), nullptr, &outStoreRegMemCmdList, before, true, skipAdd, false);

    auto eventAllocIt = std::find(residencyContainer.begin(), residencyContainer.end(), eventAllocation);
    EXPECT_EQ(residencyContainer.end(), eventAllocIt);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), commandStreamOffset),
        commandContainer.getCommandStream()->getUsed() - commandStreamOffset));

    auto storeRegMemList = findAll<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, storeRegMemList.size());
    ASSERT_NE(0u, outStoreRegMemCmdList.size());

    ASSERT_EQ(storeRegMemList.size(), outStoreRegMemCmdList.size());

    for (size_t i = 0; i < storeRegMemList.size(); i++) {
        MI_STORE_REGISTER_MEM *storeRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*storeRegMemList[i]);

        auto &cmdToPatch = outStoreRegMemCmdList[i];
        EXPECT_EQ(CommandToPatch::TimestampEventPostSyncStoreRegMem, cmdToPatch.type);
        MI_STORE_REGISTER_MEM *outStoreRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(cmdToPatch.pDestination);
        ASSERT_NE(nullptr, outStoreRegMem);

        EXPECT_EQ(storeRegMem, outStoreRegMem);

        auto cmdAddress = eventBaseAddress + cmdToPatch.offset;
        EXPECT_EQ(cmdAddress, outStoreRegMem->getMemoryAddress());
    }
}

HWTEST2_F(CommandListAppendLaunchKernel,
          givenL3EventCompactionPlatformWhenAppendKernelWithSignalScopeEventAndCmdPatchListProvidedThenDispatchSignalPostSyncCmdAndStoreInPatchList,
          IsAtLeastXeCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    commandList->dcFlushSupport = true;
    commandList->compactL3FlushEventPacket = true;

    auto &commandContainer = commandList->getCmdContainer();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = 0;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    ASSERT_NE(nullptr, event.get());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    CommandToPatch signalCmd;
    launchParams.outSyncCommand = &signalCmd;
    auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), commandStreamOffset),
        commandContainer.getCommandStream()->getUsed() - commandStreamOffset));

    auto pcList = findAll<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, pcList.size());

    PIPE_CONTROL *postSyncPipeControl = nullptr;
    for (const auto it : pcList) {
        postSyncPipeControl = genCmdCast<PIPE_CONTROL *>(*it);
        if (postSyncPipeControl->getPostSyncOperation() == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
            break;
        }
    }
    ASSERT_NE(nullptr, postSyncPipeControl);

    EXPECT_EQ(CommandToPatch::SignalEventPostSyncPipeControl, signalCmd.type);
    EXPECT_EQ(postSyncPipeControl, signalCmd.pDestination);
}

HWTEST2_F(CommandListAppendLaunchKernel,
          givenL3EventCompactionPlatformWhenAppendKernelWithTimestampSignalScopeEventAndCmdPatchListProvidedThenDispatchSignalPostSyncCmdAndStoreInPatchList,
          IsAtLeastXeCore) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    commandList->dcFlushSupport = true;
    commandList->compactL3FlushEventPacket = true;

    auto &commandContainer = commandList->getCmdContainer();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;

    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    ASSERT_NE(nullptr, event.get());

    auto eventBaseAddress = event->getGpuAddress(device);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    CommandToPatchContainer outStoreRegMemCmdList;
    launchParams.outListCommands = &outStoreRegMemCmdList;
    auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), commandStreamOffset),
        commandContainer.getCommandStream()->getUsed() - commandStreamOffset));

    auto storeRegMemList = findAll<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(0u, storeRegMemList.size());
    ASSERT_NE(0u, outStoreRegMemCmdList.size());

    size_t additionalPatchCmdsSize = commandList->kernelMemoryPrefetchEnabled() ? 1 : 0;

    ASSERT_EQ(storeRegMemList.size(), outStoreRegMemCmdList.size() - additionalPatchCmdsSize);

    for (size_t i = 0; i < storeRegMemList.size(); i++) {
        MI_STORE_REGISTER_MEM *storeRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(*storeRegMemList[i]);

        auto &cmdToPatch = outStoreRegMemCmdList[i + additionalPatchCmdsSize];
        EXPECT_EQ(CommandToPatch::TimestampEventPostSyncStoreRegMem, cmdToPatch.type);
        MI_STORE_REGISTER_MEM *outStoreRegMem = genCmdCast<MI_STORE_REGISTER_MEM *>(cmdToPatch.pDestination);
        ASSERT_NE(nullptr, outStoreRegMem);

        EXPECT_EQ(storeRegMem, outStoreRegMem);

        auto cmdAddress = eventBaseAddress + cmdToPatch.offset;
        EXPECT_EQ(cmdAddress, outStoreRegMem->getMemoryAddress());
    }
}

HWTEST2_F(CommandListAppendLaunchKernel,
          givenInOrderCmdListAndTimeStampEventWhenAppendingKernelAndEventWithOutCmdListSetThenStoreStoreDataImmClearAndSemapohreWaitPostSyncCommands,
          IsAtLeastXeCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &commandContainer = commandList->getCmdContainer();
    auto cmdStream = commandContainer.getCommandStream();

    ze_event_pool_counter_based_exp_desc_t counterBasedExtension = {ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC};
    counterBasedExtension.flags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE;

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    eventPoolDesc.pNext = &counterBasedExtension;

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    ASSERT_NE(nullptr, event.get());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    CommandToPatchContainer outCbEventCmds;
    launchParams.outListCommands = &outCbEventCmds;
    auto commandStreamOffset = cmdStream->getUsed();
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), commandStreamOffset),
        cmdStream->getUsed() - commandStreamOffset));

    size_t additionalPatchCmdsSize = commandList->kernelMemoryPrefetchEnabled() ? 1 : 0;

    ASSERT_EQ(additionalPatchCmdsSize, outCbEventCmds.size());
    auto eventBaseAddress = event->getGpuAddress(device);

    auto walker = genCmdCast<WalkerType *>(launchParams.outWalker);

    if constexpr (!FamilyType::template isHeaplessMode<WalkerType>()) {
        auto &postSync = walker->getPostSync();
        EXPECT_EQ(eventBaseAddress, postSync.getDestinationAddress());
    }
}

HWTEST2_F(CommandListAppendLaunchKernel,
          givenInOrderCmdListAndWaitEventWhenAppendingKernelAndEventWithOutWaitCmdListSetAndSkipResidencyAddThenStoreSemapohreWaitAndLoadRegisterImmCommands,
          IsAtLeastXeCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandList2 = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    result = commandList2->initialize(device, NEO::EngineGroupType::compute, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &commandContainer = commandList->getCmdContainer();
    auto cmdStream = commandContainer.getCommandStream();

    ze_event_pool_counter_based_exp_desc_t counterBasedExtension = {ZE_STRUCTURE_TYPE_COUNTER_BASED_EVENT_POOL_EXP_DESC};
    counterBasedExtension.flags = ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE;

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    eventPoolDesc.pNext = &counterBasedExtension;

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    ASSERT_NE(nullptr, event.get());

    event->updateInOrderExecState(commandList2->inOrderExecInfo, commandList2->inOrderExecInfo->getCounterValue(), commandList2->inOrderExecInfo->getAllocationOffset());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    CommandToPatchContainer outCbWaitEventCmds;
    launchParams.outListCommands = &outCbWaitEventCmds;
    launchParams.omitAddingWaitEventsResidency = true;
    auto commandStreamOffset = cmdStream->getUsed();
    auto waitEventHandle = event->toHandle();
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 1, &waitEventHandle, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), commandStreamOffset),
        cmdStream->getUsed() - commandStreamOffset));

    auto eventCompletionAddress = event->getInOrderExecInfo()->getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    auto inOrderAllocation = event->getInOrderExecInfo()->getDeviceCounterAllocation();

    size_t expectedLoadRegImmCount = FamilyType::isQwordInOrderCounter ? 2 : 0;
    size_t additionalPatchCmdsSize = commandList->kernelMemoryPrefetchEnabled() ? 1 : 0;

    size_t expectedWaitCmds = 1 + expectedLoadRegImmCount + additionalPatchCmdsSize;
    ASSERT_EQ(expectedWaitCmds, outCbWaitEventCmds.size());

    auto loadRegImmList = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedLoadRegImmCount, loadRegImmList.size());
    auto semaphoreWaitList = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, semaphoreWaitList.size());

    size_t outCbWaitEventCmdsIndex = 0;
    for (; outCbWaitEventCmdsIndex < expectedLoadRegImmCount; outCbWaitEventCmdsIndex++) {
        auto &cmd = outCbWaitEventCmds[outCbWaitEventCmdsIndex + additionalPatchCmdsSize];

        EXPECT_EQ(CommandToPatch::CbWaitEventLoadRegisterImm, cmd.type);
        ASSERT_NE(nullptr, cmd.pDestination);
        ASSERT_EQ(*loadRegImmList[outCbWaitEventCmdsIndex], cmd.pDestination);
        auto loadRegImmCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(cmd.pDestination);
        ASSERT_NE(nullptr, loadRegImmCmd);
        EXPECT_EQ(0u, cmd.inOrderPatchListIndex);
        auto registerNumber = 0x2600 + (4 * outCbWaitEventCmdsIndex);
        EXPECT_EQ(registerNumber, cmd.offset);
    }

    auto &cmd = outCbWaitEventCmds[outCbWaitEventCmdsIndex + additionalPatchCmdsSize];

    EXPECT_EQ(CommandToPatch::CbWaitEventSemaphoreWait, cmd.type);
    ASSERT_NE(nullptr, cmd.pDestination);
    ASSERT_EQ(*semaphoreWaitList[0], cmd.pDestination);
    auto semaphoreWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(cmd.pDestination);
    ASSERT_NE(nullptr, semaphoreWaitCmd);
    EXPECT_EQ(eventCompletionAddress + cmd.offset, semaphoreWaitCmd->getSemaphoreGraphicsAddress());

    if (FamilyType::isQwordInOrderCounter) {
        EXPECT_EQ(std::numeric_limits<size_t>::max(), cmd.inOrderPatchListIndex);
    } else {
        EXPECT_EQ(0u, cmd.inOrderPatchListIndex);
    }

    auto &residencyContainer = commandContainer.getResidencyContainer();

    auto eventAllocIt = std::find(residencyContainer.begin(), residencyContainer.end(), inOrderAllocation);
    if (commandList->inOrderExecInfo->getDeviceCounterAllocation() == inOrderAllocation) {
        ASSERT_NE(residencyContainer.end(), eventAllocIt);
        ++eventAllocIt;
        eventAllocIt = std::find(eventAllocIt, residencyContainer.end(), inOrderAllocation);
    }
    EXPECT_EQ(residencyContainer.end(), eventAllocIt);
}

HWTEST2_F(CommandListAppendLaunchKernel,
          givenCmdListParamHasWalkerCpuBufferWhenAppendingKernelThenCopiedWalkerHasTheSameContentAsInGfxMemory,
          IsAtLeastXeCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    bool heapless = commandList->isHeaplessModeEnabled();

    auto &commandContainer = commandList->getCmdContainer();
    auto cmdStream = commandContainer.getCommandStream();

    auto *walkerBuffer = NEO::UnitTestHelper<FamilyType>::getInitWalkerCmd(heapless);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    launchParams.cmdWalkerBuffer = walkerBuffer;
    auto commandStreamOffset = cmdStream->getUsed();
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), commandStreamOffset),
        cmdStream->getUsed() - commandStreamOffset));

    auto computeWalkerList = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, computeWalkerList.size());

    auto walker = genCmdCast<WalkerType *>(*computeWalkerList[0]);
    EXPECT_EQ(0, memcmp(walker, launchParams.cmdWalkerBuffer, sizeof(WalkerType)));
    delete static_cast<WalkerType *>(walkerBuffer);
}

HWTEST2_F(CommandListAppendLaunchKernel,
          givenCmdListParamHasExtraSpaceReserveWhenAppendingKernelThenExtraSpaceIsConsumed,
          IsAtLeastXeCore) {
    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();
    kernel.descriptor.kernelAttributes.flags.passInlineData = false;
    kernel.privateState.perThreadDataSizeForWholeThreadGroup = 0;
    kernel.privateState.crossThreadData.resize(64U, 0x0);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &commandContainer = commandList->getCmdContainer();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    launchParams.reserveExtraPayloadSpace = 1024;
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto ioh = commandContainer.getIndirectHeap(NEO::IndirectHeapType::indirectObject);

    size_t totalSize = 1024 + 64;
    size_t expectedSize = alignUp(totalSize, NEO::EncodeDispatchKernel<FamilyType>::getDefaultIOHAlignment());
    EXPECT_EQ(expectedSize, ioh->getUsed());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenNotEnoughIohSpaceWhenLaunchingKernelThenReallocateBeforePrefetch, IsAtLeastXeHpcCore) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableMemoryPrefetch.set(1);
    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();
    kernel.descriptor.kernelAttributes.flags.passInlineData = false;
    kernel.privateState.perThreadDataSizeForWholeThreadGroup = 0;
    kernel.privateState.crossThreadData.resize(64U, 0x0);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &commandContainer = commandList->getCmdContainer();
    auto cmdStream = commandContainer.getCommandStream();
    auto offset = cmdStream->getUsed();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    launchParams.reserveExtraPayloadSpace = 1024;

    auto ioh = commandContainer.getIndirectHeap(NEO::IndirectHeapType::indirectObject);
    ioh->getSpace(ioh->getMaxAvailableSpace() - (launchParams.reserveExtraPayloadSpace + kernel.getIndirectSize() + 1));

    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ioh = commandContainer.getIndirectHeap(NEO::IndirectHeapType::indirectObject);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

    auto prefetchList = find<STATE_PREFETCH *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(prefetchList, cmdList.end());

    auto statePrefetch = genCmdCast<STATE_PREFETCH *>(*prefetchList);
    ASSERT_NE(nullptr, statePrefetch);

    EXPECT_EQ(ioh->getGraphicsAllocation()->getGpuAddress(), statePrefetch->getAddress());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenDebugVariableWhenPrefetchingIsaThenLimitItsSize, IsAtLeastXeHpcCore) {
    using STATE_PREFETCH = typename FamilyType::STATE_PREFETCH;

    DebugManagerStateRestore restorer;
    debugManager.flags.EnableMemoryPrefetch.set(1);

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &commandContainer = commandList->getCmdContainer();
    auto cmdStream = commandContainer.getCommandStream();
    auto offset = cmdStream->getUsed();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto isaAlloc = kernel.getImmutableData()->getIsaGraphicsAllocation();

    uint32_t defaultIsaSize = static_cast<uint32_t>(5 * MemoryConstants::kiloByte);
    isaAlloc->setSize(defaultIsaSize);

    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams));

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        size_t prefetchedSize = 0;

        auto itor = find<STATE_PREFETCH *>(cmdList.begin(), cmdList.end());
        while (itor != cmdList.end()) {
            auto statePrefetch = genCmdCast<STATE_PREFETCH *>(*itor);
            if (statePrefetch) {
                if (statePrefetch->getAddress() >= isaAlloc->getGpuAddress() &&
                    statePrefetch->getAddress() < (isaAlloc->getGpuAddress() + isaAlloc->getUnderlyingBufferSize())) {
                    prefetchedSize += statePrefetch->getPrefetchSize() * MemoryConstants::cacheLineSize;
                }
            } else {
                break;
            }
            itor++;
        }
        EXPECT_EQ(static_cast<uint32_t>(MemoryConstants::kiloByte), prefetchedSize); // limited to 1kb
    }

    NEO::debugManager.flags.LimitIsaPrefetchSize.set(2 * MemoryConstants::kiloByte);
    offset = cmdStream->getUsed();
    ASSERT_EQ(ZE_RESULT_SUCCESS, commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams));

    {
        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(cmdList, ptrOffset(cmdStream->getCpuBase(), offset), cmdStream->getUsed() - offset));

        size_t prefetchedSize = 0;

        auto itor = find<STATE_PREFETCH *>(cmdList.begin(), cmdList.end());
        while (itor != cmdList.end()) {
            auto statePrefetch = genCmdCast<STATE_PREFETCH *>(*itor);
            if (statePrefetch) {
                if (statePrefetch->getAddress() >= isaAlloc->getGpuAddress() &&
                    statePrefetch->getAddress() < (isaAlloc->getGpuAddress() + isaAlloc->getUnderlyingBufferSize())) {
                    prefetchedSize += statePrefetch->getPrefetchSize() * MemoryConstants::cacheLineSize;
                }
            } else {
                break;
            }
            itor++;
        }
        EXPECT_EQ(static_cast<uint32_t>(2 * MemoryConstants::kiloByte), prefetchedSize); // limited to 2kb by debug variable
    }
}

HWTEST2_F(CommandListAppendLaunchKernel,
          givenFlagMakeKernelCommandViewWhenAppendKernelWithSignalEventThenDispatchNoPostSyncInViewMemoryAndNoEventAllocationAddedToResidency,
          IsAtLeastXeCore) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<FamilyType::gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device, result));
    ASSERT_NE(nullptr, event.get());

    auto eventBaseAddress = event->getGpuAddress(device);
    auto eventAlloaction = event->getAllocation(device);

    uint8_t computeWalkerHostBuffer[512];
    uint8_t payloadHostBuffer[256];

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    launchParams.makeKernelCommandView = true;
    launchParams.cmdWalkerBuffer = computeWalkerHostBuffer;
    launchParams.hostPayloadBuffer = payloadHostBuffer;

    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto walker = genCmdCast<WalkerType *>(launchParams.cmdWalkerBuffer);
    auto &postSync = walker->getPostSync();

    EXPECT_NE(eventBaseAddress, postSync.getDestinationAddress());

    auto &cmdlistResidency = commandList->getCmdContainer().getResidencyContainer();

    auto kernelAllocationIt = std::find(cmdlistResidency.begin(), cmdlistResidency.end(), eventAlloaction);
    EXPECT_EQ(kernelAllocationIt, cmdlistResidency.end());
}

} // namespace ult
} // namespace L0
