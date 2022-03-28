/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "test_traits_common.h"

namespace L0 {
namespace ult {

using CommandListTests = Test<DeviceFixture>;
HWCMDTEST_F(IGFX_XE_HP_CORE, CommandListTests, whenCommandListIsCreatedThenPCAndStateBaseAddressCmdsAreAddedAndCorrectlyProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

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
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
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

    if constexpr (FamilyType::supportsSampler) {
        auto dsh = commandContainer.getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
        EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsh->getHeapGpuBase(), cmdSba->getDynamicStateBaseAddress());
        EXPECT_EQ(dsh->getHeapSizeInPages(), cmdSba->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(cmdSba->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(cmdSba->getDynamicStateBufferSizeModifyEnable());
    }

    auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
    EXPECT_TRUE(cmdSba->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssh->getHeapGpuBase(), cmdSba->getSurfaceStateBaseAddress());

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());

    EXPECT_TRUE(cmdSba->getDisableSupportForMultiGpuPartialWritesForStatelessMessages());
    EXPECT_TRUE(cmdSba->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
}

HWTEST2_F(CommandListTests, whenCommandListIsCreatedAndProgramExtendedPipeControlPriorToNonPipelinedStateCommandIsEnabledThenPCAndStateBaseAddressCmdsAreAddedAndCorrectlyProgrammed, IsAtLeastXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(1);

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
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
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
    if constexpr (FamilyType::supportsSampler) {
        auto dsh = commandContainer.getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
        EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
        EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
        EXPECT_EQ(dsh->getHeapGpuBase(), cmdSba->getDynamicStateBaseAddress());
        EXPECT_EQ(dsh->getHeapSizeInPages(), cmdSba->getDynamicStateBufferSize());
    } else {
        EXPECT_FALSE(cmdSba->getDynamicStateBaseAddressModifyEnable());
        EXPECT_FALSE(cmdSba->getDynamicStateBufferSizeModifyEnable());
    }
    auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
    EXPECT_TRUE(cmdSba->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssh->getHeapGpuBase(), cmdSba->getSurfaceStateBaseAddress());

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());

    EXPECT_TRUE(cmdSba->getDisableSupportForMultiGpuPartialWritesForStatelessMessages());
    EXPECT_TRUE(cmdSba->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
}

using MultiTileCommandListTests = Test<MultiTileCommandListFixture<false, false, false>>;
HWCMDTEST_F(IGFX_XE_HP_CORE, MultiTileCommandListTests, givenPartitionedCommandListWhenCommandListIsCreatedThenStateBaseAddressCmdWithMultiPartialAndAtomicsCorrectlyProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::Compute, 0u, returnValue));
    EXPECT_EQ(2u, commandList->partitionCount);
    auto &commandContainer = commandList->commandContainer;

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::PARSE::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorSba = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorSba);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itorSba);

    EXPECT_FALSE(cmdSba->getDisableSupportForMultiGpuPartialWritesForStatelessMessages());
    EXPECT_TRUE(cmdSba->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
}

using CommandListTestsReserveSize = Test<DeviceFixture>;
HWTEST2_F(CommandListTestsReserveSize, givenCommandListWhenGetReserveSshSizeThen4PagesReturned, IsAtLeastXeHpCore) {
    L0::CommandListCoreFamily<gfxCoreFamily> commandList(1u);

    EXPECT_EQ(commandList.getReserveSshSize(), 4 * MemoryConstants::pageSize);
}

using CommandListAppendLaunchKernel = Test<ModuleFixture>;
HWTEST2_F(CommandListAppendLaunchKernel, givenVariousKernelsWhenUpdateStreamPropertiesIsCalledThenRequiredStateFinalStateAndCommandsToPatchAreCorrectlySet, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowMixingRegularAndCooperativeKernels.set(1);
    DebugManager.flags.AllowPatchingVfeStateInCommandLists.set(1);

    Mock<::L0::Kernel> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::Kernel> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(-1, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(-1, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());

    pCommandList->updateStreamProperties(defaultKernel, false, false);
    EXPECT_EQ(0, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(cooperativeKernel, false, true);
    pCommandList->updateStreamProperties(cooperativeKernel, false, true);
    EXPECT_EQ(1, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(1, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(defaultKernel, false, false);
    pCommandList->updateStreamProperties(cooperativeKernel, false, true);
    EXPECT_EQ(0, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(1, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(1u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(cooperativeKernel, false, true);
    pCommandList->updateStreamProperties(defaultKernel, false, false);
    pCommandList->updateStreamProperties(cooperativeKernel, false, true);
    EXPECT_EQ(1, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(1, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(2u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    pCommandList->updateStreamProperties(defaultKernel, false, false);
    pCommandList->updateStreamProperties(defaultKernel, false, false);
    pCommandList->updateStreamProperties(cooperativeKernel, false, true);
    EXPECT_EQ(0, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(1, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(1u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    EXPECT_EQ(-1, pCommandList->requiredStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(-1, pCommandList->finalStreamState.frontEndState.computeDispatchAllWalkerEnable.value);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
}

HWTEST2_F(CommandListAppendLaunchKernel, givenVariousKernelsAndPatchingDisallowedWhenUpdateStreamPropertiesIsCalledThenCommandsToPatchAreEmpty, IsAtLeastXeHpCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.AllowMixingRegularAndCooperativeKernels.set(1);
    Mock<::L0::Kernel> defaultKernel;
    auto pMockModule1 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    defaultKernel.module = pMockModule1.get();

    Mock<::L0::Kernel> cooperativeKernel;
    auto pMockModule2 = std::unique_ptr<Module>(new Mock<Module>(device, nullptr));
    cooperativeKernel.module = pMockModule2.get();
    cooperativeKernel.immutableData.kernelDescriptor->kernelAttributes.flags.usesSyncBuffer = true;

    auto pCommandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    auto result = pCommandList->initialize(device, NEO::EngineGroupType::Compute, 0u);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    pCommandList->updateStreamProperties(defaultKernel, false, false);
    pCommandList->updateStreamProperties(cooperativeKernel, false, true);
    EXPECT_EQ(0u, pCommandList->commandsToPatch.size());
    pCommandList->reset();

    DebugManager.flags.AllowPatchingVfeStateInCommandLists.set(1);
    pCommandList->updateStreamProperties(defaultKernel, false, false);
    pCommandList->updateStreamProperties(cooperativeKernel, false, true);
    EXPECT_EQ(1u, pCommandList->commandsToPatch.size());
    pCommandList->reset();
}

} // namespace ult
} // namespace L0
