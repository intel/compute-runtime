/*
 * Copyright (C) 2021-2024 Intel Corporation
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

namespace L0 {
namespace ult {

using CommandListTests = Test<DeviceFixture>;
HWCMDTEST_F(IGFX_XE_HP_CORE, CommandListTests, whenCommandListIsCreatedThenPCAndStateBaseAddressCmdsAreAddedAndCorrectlyProgrammed) {

    auto &compilerProductHelper = device->getNEODevice()->getCompilerProductHelper();
    auto isHeaplessEnabled = compilerProductHelper.isHeaplessModeEnabled();
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

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST2_F(CommandListTests, whenCommandListIsCreatedAndProgramExtendedPipeControlPriorToNonPipelinedStateCommandIsEnabledThenPCAndStateBaseAddressCmdsAreAddedAndCorrectlyProgrammed, IsAtLeastXeHpCore) {

    auto &compilerProductHelper = device->getNEODevice()->getCompilerProductHelper();
    auto isHeaplessEnabled = compilerProductHelper.isHeaplessModeEnabled();
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

    if constexpr (TestTraits<gfxCoreFamily>::isPipeControlExtendedPriorToNonPipelinedStateCommandSupported) {
        EXPECT_TRUE(cmdPc->getAmfsFlushEnable());
        EXPECT_TRUE(cmdPc->getInstructionCacheInvalidateEnable());
        EXPECT_TRUE(cmdPc->getConstantCacheInvalidationEnable());
        EXPECT_TRUE(cmdPc->getStateCacheInvalidationEnable());

        if constexpr (TestTraits<gfxCoreFamily>::isUnTypedDataPortCacheFlushSupported) {
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

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());
}

using CommandListTestsReserveSize = Test<DeviceFixture>;
HWTEST2_F(CommandListTestsReserveSize, givenCommandListWhenGetReserveSshSizeThen4PagesReturned, IsAtLeastXeHpCore) {
    L0::CommandListCoreFamily<gfxCoreFamily> commandList(1u);
    commandList.initialize(device, NEO::EngineGroupType::compute, 0u);

    EXPECT_EQ(commandList.getReserveSshSize(), 4 * MemoryConstants::pageSize);
}

using CommandListAppendLaunchKernel = Test<ModuleFixture>;
HWTEST2_F(CommandListAppendLaunchKernel, givenVariousKernelsWhenUpdateStreamPropertiesIsCalledThenRequiredStateFinalStateAndCommandsToPatchAreCorrectlySet, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;

    debugManager.flags.AllowPatchingVfeStateInCommandLists.set(1);

    Mock<::L0::KernelImp> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::KernelImp> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
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

HWTEST2_F(CommandListAppendLaunchKernel, givenVariousKernelsAndPatchingDisallowedWhenUpdateStreamPropertiesIsCalledThenCommandsToPatchAreEmpty, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;

    Mock<::L0::KernelImp> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::KernelImp> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
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
        using WalkerVariant = typename FamilyType::WalkerVariant;
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
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, input.device));

        uint64_t firstKernelEventAddress = arg.postSyncAddressZero ? 0 : event->getGpuAddress(input.device);

        ze_group_count_t groupCount{1, 1, 1};
        CmdListKernelLaunchParams launchParams = {};
        result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams, false);
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

        WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*firstWalker);
        std::visit([&arg, firstKernelEventAddress](auto &&walker) {
            using WalkerType = std::decay_t<decltype(*walker)>;
            using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
            using OPERATION = typename PostSyncType::OPERATION;
            auto &postSync = walker->getPostSync();

            EXPECT_EQ(static_cast<OPERATION>(arg.expectedWalkerPostSyncOp), postSync.getOperation());
            EXPECT_EQ(firstKernelEventAddress, postSync.getDestinationAddress());
        },
                   walkerVariant);

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
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListAppendLaunchKernelCompactL3FlushDisabledTest,
          givenAppendKernelWithSignalScopeImmediateEventWhenComputeWalkerImmediatePostsyncAndL3ImmediatePostsyncUsedThenExpectComputeWalkerAndPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = input.device->isImplicitScalingCapable() ? 3 : 1;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = 0;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using CommandListAppendLaunchKernelCompactL3FlushEnabledTest = Test<CommandListAppendLaunchKernelCompactL3FlushEventFixture<1, 0>>;

HWTEST2_F(CommandListAppendLaunchKernelCompactL3FlushEnabledTest,
          givenAppendKernelWithSignalScopeTimestampEventWhenRegisterTimestampPostsyncUsedThenExpectNoComputeWalkerAndPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 1;
    arg.expectedPostSyncPipeControls = 0;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    input.useFirstEventPacketAddress = true;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListAppendLaunchKernelCompactL3FlushEnabledTest,
          givenAppendKernelWithSignalScopeImmediateEventWhenL3ImmediatePostsyncUsedThenExpectPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 1;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using CommandListAppendLaunchKernelMultiTileCompactL3FlushDisabledTest = Test<CommandListAppendLaunchKernelCompactL3FlushEventFixture<0, 1>>;

HWTEST2_F(CommandListAppendLaunchKernelMultiTileCompactL3FlushDisabledTest,
          givenAppendMultiTileKernelWithSignalScopeTimestampEventWhenComputeWalkerTimestampPostsyncAndL3ImmediatePostsyncUsedThenExpectComputeWalkerAndPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 4;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 3;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListAppendLaunchKernelMultiTileCompactL3FlushDisabledTest,
          givenAppendMultiTileKernelWithSignalScopeImmediateEventWhenComputeWalkerImmediatePostsyncAndL3ImmediatePostsyncUsedThenExpectComputeWalkerAndPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 4;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 1;
    arg.postSyncAddressZero = false;

    input.eventPoolFlags = 0;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

using CommandListAppendLaunchKernelMultiTileCompactL3FlushEnabledTest = Test<CommandListAppendLaunchKernelCompactL3FlushEventFixture<1, 1>>;

HWTEST2_F(CommandListAppendLaunchKernelMultiTileCompactL3FlushEnabledTest,
          givenAppendMultiTileKernelWithSignalScopeTimestampEventWhenRegisterTimestampPostsyncUsedThenExpectNoComputeWalkerAndPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedPostSyncPipeControls = 0;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;
    input.useFirstEventPacketAddress = true;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
}

HWTEST2_F(CommandListAppendLaunchKernelMultiTileCompactL3FlushEnabledTest,
          givenAppendMultiTileKernelWithSignalScopeImmediateEventWhenL3ImmediatePostsyncUsedThenExpectPipeControlPostsync,
          IsXeHpOrXeHpgCore) {
    arg.expectedKernelCount = 1;
    arg.expectedPacketsInUse = 2;
    arg.expectedPostSyncPipeControls = 1;
    arg.expectedWalkerPostSyncOp = 0;
    arg.postSyncAddressZero = true;

    input.eventPoolFlags = 0;
    input.useFirstEventPacketAddress = true;

    testAppendLaunchKernelAndL3Flush<gfxCoreFamily>(input, arg);
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
        using WalkerVariant = typename FamilyType::WalkerVariant;
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
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
        ASSERT_NE(nullptr, event.get());

        ze_group_count_t groupCount{1, 1, 1};
        CmdListKernelLaunchParams launchParams = {};
        size_t sizeBefore = cmdStream->getUsed();
        result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams, false);
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

        WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*firstWalker);
        std::visit([expectedWalkerPostSyncOp](auto &&walker) {
            using WalkerType = std::decay_t<decltype(*walker)>;
            using PostSyncType = decltype(FamilyType::template getPostSyncType<WalkerType>());
            using OPERATION = typename PostSyncType::OPERATION;
            auto &postSync = walker->getPostSync();

            EXPECT_EQ(static_cast<OPERATION>(expectedWalkerPostSyncOp), postSync.getOperation());
        },
                   walkerVariant);

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
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
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
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
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
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
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
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
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
        auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
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
HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendKernelThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendKernelThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendWaitEventThenAllPacketWaitDispatched, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendWaitEventThenAllPacketWaitDispatched, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalProfilingEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventForProfiling<gfxCoreFamily>();
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(0);
}

using MultiTileCommandListSignalAllEventPacketTest = Test<CommandListSignalAllEventPacketFixture<1, 0, 0>>;
HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendKernelThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendKernelThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendWaitEventThenAllPacketWaitDispatched, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendWaitEventThenAllPacketWaitDispatched, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalProfilingEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventForProfiling<gfxCoreFamily>();
}

struct MultiTileCommandListSignalAllocLayoutTest : public MultiTileCommandListSignalAllEventPacketTest {
    void SetUp() override {
        MultiTileCommandListSignalAllEventPacketTest::SetUp();
    }
};

HWTEST2_F(MultiTileCommandListSignalAllocLayoutTest, givenDynamicLayoutEnabledWhenAppendEventForProfilingCalledThenProgramOffsetMmio, IsAtLeastXeHpCore) {
    EXPECT_NE(ImplicitScalingDispatch<FamilyType>::getImmediateWritePostSyncOffset(), ImplicitScalingDispatch<FamilyType>::getTimeStampPostSyncOffset());

    testAppendSignalEventForProfiling<gfxCoreFamily>();
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventWithSinglePacketThenAllPacketResetDispatchedUsingNonPartitionedWrite, IsAtLeastXeHpCore) {
    alignEventPacketsForReset = false;

    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventWithSinglePacketThenAllPacketResetDispatchedUsingNonPartitionedWrite, IsAtLeastXeHpCore) {
    alignEventPacketsForReset = false;

    testAppendResetEvent<gfxCoreFamily>(0);
}

using CommandListSignalAllEventPacketForCompactEventTest = Test<CommandListSignalAllEventPacketFixture<0, 1, 0>>;
HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendKernelThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendKernelThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendWaitEventThenAllPacketWaitDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendWaitEventThenAllPacketWaitDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalProfilingEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventForProfiling<gfxCoreFamily>();
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(0);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(0);
}

using MultiTileCommandListSignalAllEventPacketForCompactEventTest = Test<CommandListSignalAllEventPacketFixture<1, 1, 0>>;
HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendKernelThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendKernelThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendKernel<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendWaitEventThenAllPacketWaitDispatchedAsDefaultActiveSinglePacket, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendWaitEventThenAllPacketWaitDispatchedAsDefaultActiveSinglePacket, IsAtLeastXeHpCore) {
    testAppendWaitEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalProfilingEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventForProfiling<gfxCoreFamily>();
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventWithSinglePacketThenAllPacketResetDispatchedUsingNonPartitionedWrite, IsAtLeastXeHpCore) {
    alignEventPacketsForReset = false;

    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventWithSinglePacketThenAllPacketResetDispatchedUsingNonPartitionedWrite, IsAtLeastXeHpCore) {
    alignEventPacketsForReset = false;

    testAppendResetEvent<gfxCoreFamily>(0);
}

using CopyCommandListSignalAllEventPacketTest = Test<CommandListSignalAllEventPacketFixture<0, 0, 1>>;
HWTEST2_F(CopyCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(0);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(0);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(0);
}

using MultiTileCopyCommandListSignalAllEventPacketTest = Test<CommandListSignalAllEventPacketFixture<1, 0, 1>>;
HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatched, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatched, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(0);
}

using CopyCommandListSignalAllEventPacketForCompactEventTest = Test<CommandListSignalAllEventPacketFixture<0, 1, 1>>;
HWTEST2_F(CopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(0);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(0);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(CopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(0);
}

using MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest = Test<CommandListSignalAllEventPacketFixture<1, 1, 1>>;
HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendSignalEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEvent<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalImmediateEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(0);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsEventWhenAppendSignalTimestampEventThenAllPacketCompletionDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendSignalEventPostAppendCall<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsTimestampEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP);
}

HWTEST2_F(MultiTileCopyCommandListSignalAllEventPacketForCompactEventTest, givenSignalPacketsImmediateEventWhenAppendResetEventThenAllPacketResetDispatchNotNeeded, IsAtLeastXeHpCore) {
    testAppendResetEvent<gfxCoreFamily>(0);
}

using RayTracingMatcher = IsAtLeastXeHpCore;

using CommandListAppendLaunchRayTracingKernelTest = Test<CommandListAppendLaunchRayTracingKernelFixture>;

HWTEST2_F(CommandListAppendLaunchRayTracingKernelTest, givenKernelUsingRayTracingWhenAppendLaunchKernelIsCalledThenSuccessIsReturned, RayTracingMatcher) {
    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    kernel.immutableData.kernelDescriptor->kernelAttributes.flags.hasRTCalls = true;

    neoDevice->rtMemoryBackedBuffer = nullptr;
    CmdListKernelLaunchParams launchParams = {};
    result = pCommandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_NE(ZE_RESULT_SUCCESS, result);

    neoDevice->rtMemoryBackedBuffer = buffer1;
    result = pCommandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    neoDevice->rtMemoryBackedBuffer = nullptr;
}

HWTEST2_F(CommandListAppendLaunchRayTracingKernelTest, givenDcFlushMitigationWhenAppendLaunchKernelWithRayTracingIsCalledThenRequireDcFlush, RayTracingMatcher) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AllowDcFlush.set(0);

    Mock<::L0::KernelImp> kernel;
    auto pMockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = pMockModule.get();

    kernel.setGroupSize(4, 1, 1);
    ze_group_count_t groupCount{8, 1, 1};
    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::compute, 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    kernel.immutableData.kernelDescriptor->kernelAttributes.flags.hasRTCalls = true;
    neoDevice->rtMemoryBackedBuffer = buffer1;
    CmdListKernelLaunchParams launchParams = {};

    result = pCommandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(pCommandList->requiresDcFlushForDcMitigation, device->getProductHelper().isDcFlushMitigated());

    neoDevice->rtMemoryBackedBuffer = nullptr;
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
    } else {
        if (size == 0) {
            return;
        }
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
        auto &btdStateBody = btdStateCmd->getBtdStateBody();
        EXPECT_EQ(gpuVa, btdStateBody.getMemoryBackedBufferBasePointer());

        btdCommandFound = true;
    }

    EXPECT_EQ(expectToFind, btdCommandFound);
}

HWTEST2_F(RayTracingCmdListTest,
          givenRayTracingKernelWhenRegularCmdListExecutedAndRegularExecutedAgainThenDispatch3dBtdCommandOnceMakeResidentTwiceAndPipeControlWithStateCacheFlushAfterWalker,
          RayTracingMatcher) {

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->storeMakeResidentAllocations = true;

    auto &containerRegular = commandList->getCmdContainer();
    auto &cmdStreamRegular = *containerRegular.getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t regularSizeBefore = cmdStreamRegular.getUsed();
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    size_t regularSizeAfter = cmdStreamRegular.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    findStateCacheFlushPipeControlAfterWalker<FamilyType>(cmdStreamRegular, regularSizeBefore, regularSizeAfter - regularSizeBefore);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueSizeBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueSizeAfter = cmdQueueStream.getUsed();

    // find 3D BTD command - first time use
    find3dBtdCommand<FamilyType>(cmdQueueStream, queueSizeBefore, queueSizeAfter - queueSizeBefore, rtAllocationAddress, true);

    uint32_t residentCount = 1;
    ultCsr->isMadeResident(rtAllocation, residentCount);

    queueSizeBefore = cmdQueueStream.getUsed();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    queueSizeAfter = cmdQueueStream.getUsed();

    // no find - subsequent dispatch
    find3dBtdCommand<FamilyType>(cmdQueueStream, queueSizeBefore, queueSizeAfter - queueSizeBefore, rtAllocationAddress, false);

    residentCount++;
    ultCsr->isMadeResident(rtAllocation, residentCount);
}

HWTEST2_F(RayTracingCmdListTest,
          givenDcFlushMitigationWhenRegularAppendLaunchKernelAndExecuteThenRegisterDcFlushForDcFlushMitigation,
          RayTracingMatcher) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AllowDcFlush.set(0);

    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(ultCsr->registeredDcFlushForDcFlushMitigation, device->getProductHelper().isDcFlushMitigated());
}

HWTEST2_F(RayTracingCmdListTest,
          givenRayTracingKernelWhenRegularCmdListExecutedAndImmediateExecutedAgainThenDispatch3dBtdCommandOnceMakeResidentTwiceAndPipeControlWithStateCacheFlushAfterWalker,
          RayTracingMatcher) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->storeMakeResidentAllocations = true;

    auto &containerRegular = commandList->getCmdContainer();
    auto &cmdStreamRegular = *containerRegular.getCommandStream();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t regularSizeBefore = cmdStreamRegular.getUsed();
    auto result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    size_t regularSizeAfter = cmdStreamRegular.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    findStateCacheFlushPipeControlAfterWalker<FamilyType>(cmdStreamRegular, regularSizeBefore, regularSizeAfter - regularSizeBefore);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueSizeBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr);
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
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->storeMakeResidentAllocations = true;

    auto &containerImmediate = commandListImmediate->getCmdContainer();
    auto &cmdStreamImmediate = *containerImmediate.getCommandStream();

    auto &csrStream = ultCsr->commandStream;

    size_t immediateSizeBefore = cmdStreamImmediate.getUsed();
    size_t csrBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t csrAfter = csrStream.getUsed();
    size_t immediateSizeAfter = cmdStreamImmediate.getUsed();

    findStateCacheFlushPipeControlAfterWalker<FamilyType>(cmdStreamImmediate, immediateSizeBefore, immediateSizeAfter - immediateSizeBefore);

    find3dBtdCommand<FamilyType>(csrStream, csrBefore, csrAfter - csrBefore, rtAllocationAddress, true);

    uint32_t residentCount = 1;
    ultCsr->isMadeResident(rtAllocation, residentCount);

    immediateSizeBefore = cmdStreamImmediate.getUsed();
    csrBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    csrAfter = csrStream.getUsed();
    immediateSizeAfter = cmdStreamImmediate.getUsed();

    findStateCacheFlushPipeControlAfterWalker<FamilyType>(cmdStreamImmediate, immediateSizeBefore, immediateSizeAfter - immediateSizeBefore);

    find3dBtdCommand<FamilyType>(csrStream, csrBefore, csrAfter - csrBefore, rtAllocationAddress, false);

    residentCount++;
    ultCsr->isMadeResident(rtAllocation, residentCount);
}

HWTEST2_F(RayTracingCmdListTest,
          givenDcFlushMitigationWhenImmediateAppendLaunchKernelThenRegisterDcFlushForDcFlushMitigation,
          RayTracingMatcher) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AllowDcFlush.set(0);

    commandListImmediate->isSyncModeQueue = true;
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ultCsr->registeredDcFlushForDcFlushMitigation, device->getProductHelper().isDcFlushMitigated());
}

HWTEST2_F(RayTracingCmdListTest,
          givenRayTracingKernelWhenImmediateCmdListExecutedAndRegularExecutedAgainThenDispatch3dBtdCommandOnceMakeResidentTwiceAndPipeControlWithStateCacheFlushAfterWalker,
          RayTracingMatcher) {
    auto ultCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(commandQueue->getCsr());
    ultCsr->storeMakeResidentAllocations = true;

    auto &containerImmediate = commandListImmediate->getCmdContainer();
    auto &cmdStreamImmediate = *containerImmediate.getCommandStream();

    auto &csrStream = ultCsr->commandStream;

    size_t immediateSizeBefore = cmdStreamImmediate.getUsed();
    size_t csrBefore = csrStream.getUsed();
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    size_t regularSizeAfter = cmdStreamRegular.getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    findStateCacheFlushPipeControlAfterWalker<FamilyType>(cmdStreamRegular, regularSizeBefore, regularSizeAfter - regularSizeBefore);

    result = commandList->close();
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &cmdQueueStream = commandQueue->commandStream;

    size_t queueSizeBefore = cmdQueueStream.getUsed();
    ze_command_list_handle_t cmdListHandle = commandList->toHandle();
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    size_t queueSizeAfter = cmdQueueStream.getUsed();

    find3dBtdCommand<FamilyType>(cmdQueueStream, queueSizeBefore, queueSizeAfter - queueSizeBefore, rtAllocationAddress, false);

    residentCount++;
    ultCsr->isMadeResident(rtAllocation, residentCount);
}

using ImmediateFlushTaskGlobalStatelessCmdListTest = Test<ImmediateFlushTaskGlobalStatelessCmdListFixture>;

HWTEST2_F(ImmediateFlushTaskGlobalStatelessCmdListTest,
          givenImmediateFlushOnGlobalStatelessWhenAppendingKernelThenExpectStateBaseAddressCommandDispatchedOnce,
          IsAtLeastXeHpCore) {
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
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
          IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    if (neoDevice->getProductHelper().isNewCoherencyModelSupported()) {
        GTEST_SKIP();
    }

    auto &csrImmediate = neoDevice->getUltCommandStreamReceiver<FamilyType>();
    auto &csrStream = csrImmediate.commandStream;

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    size_t csrUsedBefore = csrStream.getUsed();
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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

    kernel->kernelRequiresUncachedMocsCount++;

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
          IsAtLeastXeHpCore) {
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
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
          IsAtLeastXeHpCore) {
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
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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

    kernel->kernelRequiresUncachedMocsCount++;

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
          IsAtLeastXeHpCore) {
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
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
          IsAtLeastXeHpCore) {
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
          IsAtLeastXeHpCore) {
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
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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

    result = commandList->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
    result = commandQueue->executeCommandLists(1, &cmdListHandle, nullptr, true, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);

    auto &csrBaseAddressState = csrImmediate.getStreamProperties().stateBaseAddress;
    EXPECT_EQ(ssRegularBaseAddress, static_cast<uint64_t>(csrBaseAddressState.surfaceStateBaseAddress.value));
    EXPECT_EQ(dsRegularBaseAddress, static_cast<uint64_t>(csrBaseAddressState.dynamicStateBaseAddress.value));

    csrUsedBefore = csrStream.getUsed();
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
          IsAtLeastXeHpCore) {
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
    auto result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
    result = commandListImmediate->appendLaunchKernel(kernel->toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
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
HWTEST2_F(CommandListCreate, givenPlatformSupportsHdcUntypedCacheFlushWhenAppendWriteGlobalTimestampThenExpectNoCacheFlushInPostSyncCommand, IsAtLeastXeHpCore) {
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

HWTEST2_F(CommandListCreate, givenAppendSignalEventWhenSkipAddToResidencyTrueThenEventAllocationNotAddedToResidency, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
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
          IsAtLeastXeHpCore) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
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
          IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    ASSERT_NE(nullptr, event.get());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    CommandToPatch signalCmd;
    launchParams.outSyncCommand = &signalCmd;
    auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams, false);
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
          IsAtLeastXeHpCore) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    ASSERT_NE(nullptr, event.get());

    auto eventBaseAddress = event->getGpuAddress(device);

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    CommandToPatchContainer outStoreRegMemCmdList;
    launchParams.outListCommands = &outStoreRegMemCmdList;
    auto commandStreamOffset = commandContainer.getCommandStream()->getUsed();
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

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
          givenInOrderCmdListAndTimeStampEventWhenAppendingKernelAndEventWithOutCmdListSetThenStoreStoreDataImmClearAndSemapohreWaitPostSyncCommands,
          IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using WalkerVariant = typename FamilyType::WalkerVariant;

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    bool heapless = commandList->isHeaplessModeEnabled();

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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    ASSERT_NE(nullptr, event.get());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    CommandToPatchContainer outCbEventCmds;
    launchParams.outListCommands = &outCbEventCmds;
    auto commandStreamOffset = cmdStream->getUsed();
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), commandStreamOffset),
        cmdStream->getUsed() - commandStreamOffset));

    auto eventCompletionAddress = event->getCompletionFieldGpuAddress(device);

    ASSERT_EQ(heapless ? 0u : 2u, outCbEventCmds.size());
    size_t expectedSdi = heapless ? 0 : commandList->inOrderAtomicSignalingEnabled ? 1
                                                                                   : 2;

    auto storeDataImmList = findAll<MI_STORE_DATA_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedSdi, storeDataImmList.size());
    auto computeWalkerList = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, computeWalkerList.size());
    auto semaphoreWaitList = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(heapless ? 0u : 1u, semaphoreWaitList.size());

    if (!heapless) {
        EXPECT_EQ(CommandToPatch::CbEventTimestampClearStoreDataImm, outCbEventCmds[0].type);
        EXPECT_EQ(*storeDataImmList[0], outCbEventCmds[0].pDestination);
        auto storeDataImmCmd = genCmdCast<MI_STORE_DATA_IMM *>(outCbEventCmds[0].pDestination);
        ASSERT_NE(nullptr, storeDataImmCmd);
        EXPECT_EQ(eventCompletionAddress, storeDataImmCmd->getAddress());
    }
    EXPECT_EQ(launchParams.outWalker, *computeWalkerList[0]);

    ASSERT_NE(nullptr, launchParams.outWalker);
    auto eventBaseAddress = event->getGpuAddress(device);

    WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(launchParams.outWalker);
    std::visit([eventBaseAddress](auto &&walker) {
        using WalkerType = std::decay_t<decltype(*walker)>;

        if constexpr (!FamilyType::template isHeaplessMode<WalkerType>()) {
            auto &postSync = walker->getPostSync();
            EXPECT_EQ(eventBaseAddress, postSync.getDestinationAddress());
        }
    },
               walkerVariant);

    if (!heapless) {

        EXPECT_EQ(CommandToPatch::CbEventTimestampPostSyncSemaphoreWait, outCbEventCmds[1].type);
        EXPECT_EQ(*semaphoreWaitList[0], outCbEventCmds[1].pDestination);
        auto semaphoreWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(outCbEventCmds[1].pDestination);
        ASSERT_NE(nullptr, semaphoreWaitCmd);
        EXPECT_EQ(eventCompletionAddress, semaphoreWaitCmd->getSemaphoreGraphicsAddress());
    }
}

HWTEST2_F(CommandListAppendLaunchKernel,
          givenInOrderCmdListAndWaitEventWhenAppendingKernelAndEventWithOutWaitCmdListSetAndSkipResidencyAddThenStoreSemapohreWaitAndLoadRegisterImmCommands,
          IsAtLeastXeHpCore) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, ZE_COMMAND_LIST_FLAG_IN_ORDER);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto commandList2 = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
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
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
    ASSERT_NE(nullptr, event.get());

    event->updateInOrderExecState(commandList2->inOrderExecInfo, commandList2->inOrderExecInfo->getCounterValue(), commandList2->inOrderExecInfo->getAllocationOffset());

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    CommandToPatchContainer outCbWaitEventCmds;
    launchParams.outListCommands = &outCbWaitEventCmds;
    launchParams.omitAddingWaitEventsResidency = true;
    auto commandStreamOffset = cmdStream->getUsed();
    auto waitEventHandle = event->toHandle();
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 1, &waitEventHandle, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), commandStreamOffset),
        cmdStream->getUsed() - commandStreamOffset));

    auto eventCompletionAddress = event->getInOrderExecInfo()->getBaseDeviceAddress() + event->getInOrderAllocationOffset();
    auto inOrderAllocation = event->getInOrderExecInfo()->getDeviceCounterAllocation();

    size_t expectedLoadRegImmCount = FamilyType::isQwordInOrderCounter ? 2 : 0;

    size_t expectedWaitCmds = 1 + expectedLoadRegImmCount;
    ASSERT_EQ(expectedWaitCmds, outCbWaitEventCmds.size());

    auto loadRegImmList = findAll<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(expectedLoadRegImmCount, loadRegImmList.size());
    auto semaphoreWaitList = findAll<MI_SEMAPHORE_WAIT *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, semaphoreWaitList.size());

    size_t outCbWaitEventCmdsIndex = 0;
    for (; outCbWaitEventCmdsIndex < expectedLoadRegImmCount; outCbWaitEventCmdsIndex++) {
        EXPECT_EQ(CommandToPatch::CbWaitEventLoadRegisterImm, outCbWaitEventCmds[outCbWaitEventCmdsIndex].type);
        ASSERT_NE(nullptr, outCbWaitEventCmds[outCbWaitEventCmdsIndex].pDestination);
        ASSERT_EQ(*loadRegImmList[outCbWaitEventCmdsIndex], outCbWaitEventCmds[outCbWaitEventCmdsIndex].pDestination);
        auto loadRegImmCmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(outCbWaitEventCmds[outCbWaitEventCmdsIndex].pDestination);
        ASSERT_NE(nullptr, loadRegImmCmd);
        EXPECT_EQ(0u, outCbWaitEventCmds[outCbWaitEventCmdsIndex].inOrderPatchListIndex);
        auto registerNumber = 0x2600 + (4 * outCbWaitEventCmdsIndex);
        EXPECT_EQ(registerNumber, outCbWaitEventCmds[outCbWaitEventCmdsIndex].offset);
    }

    EXPECT_EQ(CommandToPatch::CbWaitEventSemaphoreWait, outCbWaitEventCmds[outCbWaitEventCmdsIndex].type);
    ASSERT_NE(nullptr, outCbWaitEventCmds[outCbWaitEventCmdsIndex].pDestination);
    ASSERT_EQ(*semaphoreWaitList[0], outCbWaitEventCmds[outCbWaitEventCmdsIndex].pDestination);
    auto semaphoreWaitCmd = genCmdCast<MI_SEMAPHORE_WAIT *>(outCbWaitEventCmds[outCbWaitEventCmdsIndex].pDestination);
    ASSERT_NE(nullptr, semaphoreWaitCmd);
    EXPECT_EQ(eventCompletionAddress + outCbWaitEventCmds[outCbWaitEventCmdsIndex].offset, semaphoreWaitCmd->getSemaphoreGraphicsAddress());

    if (FamilyType::isQwordInOrderCounter) {
        EXPECT_EQ(std::numeric_limits<size_t>::max(), outCbWaitEventCmds[outCbWaitEventCmdsIndex].inOrderPatchListIndex);
    } else {
        EXPECT_EQ(0u, outCbWaitEventCmds[outCbWaitEventCmdsIndex].inOrderPatchListIndex);
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
          IsAtLeastXeHpCore) {
    using WalkerVariant = typename FamilyType::WalkerVariant;

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
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
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(cmdStream->getCpuBase(), commandStreamOffset),
        cmdStream->getUsed() - commandStreamOffset));

    auto computeWalkerList = NEO::UnitTestHelper<FamilyType>::findAllWalkerTypeCmds(cmdList.begin(), cmdList.end());
    ASSERT_EQ(1u, computeWalkerList.size());

    WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(*computeWalkerList[0]);
    std::visit([&launchParams, &walkerBuffer](auto &&walker) {
        using WalkerType = std::decay_t<decltype(*walker)>;
        EXPECT_EQ(0, memcmp(walker, launchParams.cmdWalkerBuffer, sizeof(WalkerType)));
        delete static_cast<WalkerType *>(walkerBuffer);
    },
               walkerVariant);
}

HWTEST2_F(CommandListAppendLaunchKernel,
          givenCmdListParamHasExtraSpaceReserveWhenAppendingKernelThenExtraSpaceIsConsumed,
          IsAtLeastXeHpCore) {
    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();
    kernel.descriptor.kernelAttributes.flags.passInlineData = false;
    kernel.perThreadDataSizeForWholeThreadGroup = 0;
    kernel.crossThreadDataSize = 64;
    kernel.crossThreadData = std::make_unique<uint8_t[]>(kernel.crossThreadDataSize);

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto &commandContainer = commandList->getCmdContainer();

    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};
    launchParams.reserveExtraPayloadSpace = 1024;
    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto ioh = commandContainer.getIndirectHeap(NEO::IndirectHeapType::indirectObject);

    size_t totalSize = 1024 + 64;
    size_t expectedSize = alignUp(totalSize, NEO::EncodeDispatchKernel<FamilyType>::getDefaultIOHAlignment());
    EXPECT_EQ(expectedSize, ioh->getUsed());
}

HWTEST2_F(CommandListAppendLaunchKernel,
          givenFlagMakeKernelCommandViewWhenAppendKernelWithSignalEventThenDispatchNoPostSyncInViewMemoryAndNoEventAllocationAddedToResidency,
          IsAtLeastXeHpCore) {
    using WalkerVariant = typename FamilyType::WalkerVariant;

    Mock<::L0::KernelImp> kernel;
    auto mockModule = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    kernel.module = mockModule.get();

    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = commandList->initialize(device, NEO::EngineGroupType::compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;

    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));
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

    result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event->toHandle(), 0, nullptr, launchParams, false);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    WalkerVariant walkerVariant = NEO::UnitTestHelper<FamilyType>::getWalkerVariant(launchParams.cmdWalkerBuffer);
    std::visit([eventBaseAddress](auto &&walker) {
        auto &postSync = walker->getPostSync();

        EXPECT_NE(eventBaseAddress, postSync.getDestinationAddress());
    },
               walkerVariant);

    auto &cmdlistResidency = commandList->getCmdContainer().getResidencyContainer();

    auto kernelAllocationIt = std::find(cmdlistResidency.begin(), cmdlistResidency.end(), eventAlloaction);
    EXPECT_EQ(kernelAllocationIt, cmdlistResidency.end());
}

} // namespace ult
} // namespace L0
