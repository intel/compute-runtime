/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/state_base_address_tests.h"

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/state_base_address_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "encode_surface_state_args.h"

HWTEST2_F(SbaTest, WhenAppendStateBaseAddressParametersIsCalledThenSBACmdHasBindingSurfaceStateProgrammed, IsGen12LP) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    STATE_BASE_ADDRESS stateBaseAddress;
    stateBaseAddress.setBindlessSurfaceStateSize(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddress(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddressModifyEnable(false);

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&stateBaseAddress, nullptr, &ssh, nullptr, nullptr);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(ssh.getMaxAvailableSpace() / 64 - 1, stateBaseAddress.getBindlessSurfaceStateSize());
    EXPECT_EQ(ssh.getHeapGpuBase(), stateBaseAddress.getBindlessSurfaceStateBaseAddress());
    EXPECT_TRUE(stateBaseAddress.getBindlessSurfaceStateBaseAddressModifyEnable());
}

HWTEST2_F(SbaTest, WhenProgramStateBaseAddressParametersIsCalledThenSBACmdHasBindingSurfaceStateProgrammed, MatchAny) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    STATE_BASE_ADDRESS stateBaseAddress;
    stateBaseAddress.setBindlessSurfaceStateSize(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddress(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddressModifyEnable(false);

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(commandStream.getSpace(0));
    *cmd = stateBaseAddress;

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(cmd, pDevice->getGmmHelper(), &ssh, nullptr, nullptr);

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_EQ(ssh.getMaxAvailableSpace() / 64 - 1, cmd->getBindlessSurfaceStateSize());
    EXPECT_EQ(ssh.getHeapGpuBase(), cmd->getBindlessSurfaceStateBaseAddress());
    EXPECT_TRUE(cmd->getBindlessSurfaceStateBaseAddressModifyEnable());

    EXPECT_TRUE(cmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssh.getHeapGpuBase(), cmd->getSurfaceStateBaseAddress());
}

HWTEST2_F(SbaTest,
          givenProgramSurfaceStateBaseAddressUsingHeapBaseWhenOverrideSurfaceStateBaseAddressUsedThenSbaDispatchedWithOverrideValue, MatchAny) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    constexpr uint64_t surfaceStateBaseAddress = 0xBADA550000;

    STATE_BASE_ADDRESS cmd;

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&cmd, pDevice->getGmmHelper(), &ssh, nullptr, nullptr);
    args.surfaceStateBaseAddress = surfaceStateBaseAddress;
    args.overrideSurfaceStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_TRUE(cmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(surfaceStateBaseAddress, cmd.getSurfaceStateBaseAddress());
}

using SbaForBindlessTests = SbaTest;

HWTEST2_F(SbaForBindlessTests, givenGlobalBindlessBaseAddressWhenProgramStateBaseAddressThenSbaProgrammedCorrectly, MatchAny) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    constexpr uint64_t globalBindlessHeapsBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(cmd, pDevice->getGmmHelper());
    args.globalHeapsBaseAddress = globalBindlessHeapsBaseAddress;
    args.useGlobalHeapsBaseAddress = true;
    args.ssh = &ssh;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_TRUE(cmd->getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(globalBindlessHeapsBaseAddress, cmd->getBindlessSurfaceStateBaseAddress());

    auto surfaceStateCount = StateBaseAddressHelper<FamilyType>::getMaxBindlessSurfaceStates();
    EXPECT_EQ(surfaceStateCount, cmd->getBindlessSurfaceStateSize());

    EXPECT_TRUE(cmd->getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(globalBindlessHeapsBaseAddress, cmd->getBindlessSurfaceStateBaseAddress());

    EXPECT_TRUE(cmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(globalBindlessHeapsBaseAddress, cmd->getDynamicStateBaseAddress());

    EXPECT_TRUE(cmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_NE(globalBindlessHeapsBaseAddress, cmd->getSurfaceStateBaseAddress());
    EXPECT_EQ(ssh.getHeapGpuBase(), cmd->getSurfaceStateBaseAddress());
}

HWTEST2_F(SbaForBindlessTests,
          givenGlobalBindlessBaseAddressOverriddenSurfaceStateBaseAddressWhenProgramStateBaseAddressThenSbaProgrammedCorrectly, MatchAny) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    constexpr uint64_t globalBindlessHeapsBaseAddress = 0x12340000;
    constexpr uint64_t surfaceStateBaseAddress = 0xBADA550000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(cmd, pDevice->getGmmHelper());
    args.globalHeapsBaseAddress = globalBindlessHeapsBaseAddress;
    args.surfaceStateBaseAddress = surfaceStateBaseAddress;
    args.useGlobalHeapsBaseAddress = true;
    args.overrideSurfaceStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_TRUE(cmd->getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(globalBindlessHeapsBaseAddress, cmd->getBindlessSurfaceStateBaseAddress());

    auto surfaceStateCount = StateBaseAddressHelper<FamilyType>::getMaxBindlessSurfaceStates();
    EXPECT_EQ(surfaceStateCount, cmd->getBindlessSurfaceStateSize());

    EXPECT_TRUE(cmd->getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(globalBindlessHeapsBaseAddress, cmd->getBindlessSurfaceStateBaseAddress());

    EXPECT_TRUE(cmd->getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmd->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(globalBindlessHeapsBaseAddress, cmd->getDynamicStateBaseAddress());

    EXPECT_TRUE(cmd->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(surfaceStateBaseAddress, cmd->getSurfaceStateBaseAddress());
}

using IohSupported = IsGen12LP;

HWTEST2_F(SbaForBindlessTests, givenGlobalBindlessBaseAddressWhenPassingIndirectBaseAddressThenIndirectBaseAddressIsSet, IohSupported) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    constexpr uint64_t globalBindlessHeapsBaseAddress = 0x12340000;
    constexpr uint64_t indirectObjectBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(cmd, pDevice->getGmmHelper());
    args.globalHeapsBaseAddress = globalBindlessHeapsBaseAddress;
    args.indirectObjectHeapBaseAddress = indirectObjectBaseAddress;
    args.useGlobalHeapsBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_EQ(cmd->getIndirectObjectBaseAddress(), indirectObjectBaseAddress);
}

HWTEST2_F(SbaTest, givenSbaWhenOverrideBindlessSurfaceBaseIsFalseThenBindlessSurfaceBaseIsNotSet, MatchAny) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    STATE_BASE_ADDRESS stateBaseAddress;
    stateBaseAddress.setBindlessSurfaceStateSize(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddress(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddressModifyEnable(false);

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&stateBaseAddress, pDevice->getRootDeviceEnvironment().getGmmHelper());
    args.useGlobalHeapsBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(0u, stateBaseAddress.getBindlessSurfaceStateBaseAddress());
}

HWTEST2_F(SbaTest, givenGlobalBindlessBaseAddressWhenSshIsPassedThenBindlessSurfaceBaseIsGlobalHeapBase, MatchAny) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    constexpr uint64_t globalBindlessHeapsBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(cmd, pDevice->getGmmHelper(), &ssh, nullptr, nullptr);
    args.globalHeapsBaseAddress = globalBindlessHeapsBaseAddress;
    args.useGlobalHeapsBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_EQ(cmd->getBindlessSurfaceStateBaseAddress(), globalBindlessHeapsBaseAddress);
}
HWTEST2_F(SbaTest, givenSurfaceStateHeapWhenNotUsingGlobalHeapBaseThenBindlessSurfaceBaseIsSshBase, MatchAny) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    constexpr uint64_t globalBindlessHeapsBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(cmd, pDevice->getGmmHelper(), &ssh, nullptr, nullptr);
    args.globalHeapsBaseAddress = globalBindlessHeapsBaseAddress;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_EQ(ssh.getHeapGpuBase(), cmd->getBindlessSurfaceStateBaseAddress());
}

HWTEST2_F(SbaTest, givenNotUsedGlobalHeapBaseAndSshPassedWhenBindlessSurfStateBaseIsPassedThenBindlessSurfaceBaseIsSetToPassedValue, MatchAny) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    constexpr uint64_t globalBindlessHeapsBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(cmd, pDevice->getGmmHelper(), &ssh, nullptr, nullptr);
    args.bindlessSurfaceStateBaseAddress = globalBindlessHeapsBaseAddress;
    args.useGlobalHeapsBaseAddress = false;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_EQ(cmd->getBindlessSurfaceStateBaseAddress(), globalBindlessHeapsBaseAddress);
    EXPECT_NE(ssh.getHeapGpuBase(), globalBindlessHeapsBaseAddress);
}

HWTEST2_F(SbaTest, givenStateBaseAddressAndDebugFlagSetWhenAppendExtraCacheSettingsThenNothingChanged, IsGen12LP) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto stateBaseAddress = FamilyType::cmdInitStateBaseAddress;
    auto expectedStateBaseAddress = FamilyType::cmdInitStateBaseAddress;
    DebugManagerStateRestore restore;

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&stateBaseAddress, pDevice->getGmmHelper(), &ssh, nullptr, nullptr);

    StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
    EXPECT_EQ(0, memcmp(&stateBaseAddress, &expectedStateBaseAddress, sizeof(STATE_BASE_ADDRESS)));

    debugManager.flags.ForceStatelessL1CachingPolicy.set(2);
    StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
    EXPECT_EQ(0, memcmp(&stateBaseAddress, &expectedStateBaseAddress, sizeof(STATE_BASE_ADDRESS)));

    debugManager.flags.ForceAllResourcesUncached.set(true);
    StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
    EXPECT_EQ(0, memcmp(&stateBaseAddress, &expectedStateBaseAddress, sizeof(STATE_BASE_ADDRESS)));
}

HWTEST2_F(SbaTest, givenStateBaseAddressAndDebugFlagSetWhenAppendExtraCacheSettingsThenProgramCorrectL1CachePolicy, IsAtLeastXeCore) {
    auto stateBaseAddress = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&stateBaseAddress, pDevice->getGmmHelper(), &ssh, nullptr, nullptr);
    auto &productHelper = getHelper<ProductHelper>();
    {
        DebugManagerStateRestore restore;
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, stateBaseAddress.getL1CacheControlCachePolicy());

        debugManager.flags.ForceStatelessL1CachingPolicy.set(2);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WB, stateBaseAddress.getL1CacheControlCachePolicy());

        debugManager.flags.ForceStatelessL1CachingPolicy.set(3);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WT, stateBaseAddress.getL1CacheControlCachePolicy());

        debugManager.flags.ForceStatelessL1CachingPolicy.set(4);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WS, stateBaseAddress.getL1CacheControlCachePolicy());

        debugManager.flags.ForceAllResourcesUncached.set(true);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_UC, stateBaseAddress.getL1CacheControlCachePolicy());
    }
    args.isDebuggerActive = true;
    {
        DebugManagerStateRestore restore;
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, stateBaseAddress.getL1CacheControlCachePolicy());

        debugManager.flags.ForceStatelessL1CachingPolicy.set(2);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WB, stateBaseAddress.getL1CacheControlCachePolicy());

        debugManager.flags.ForceStatelessL1CachingPolicy.set(3);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WT, stateBaseAddress.getL1CacheControlCachePolicy());

        debugManager.flags.ForceStatelessL1CachingPolicy.set(4);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WS, stateBaseAddress.getL1CacheControlCachePolicy());

        debugManager.flags.ForceAllResourcesUncached.set(true);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_UC, stateBaseAddress.getL1CacheControlCachePolicy());
    }
}

HWTEST2_F(SbaTest, givenDebugFlagSetWhenAppendingSbaThenProgramCorrectL1CachePolicy, IsAtLeastXeCore) {
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;

    struct {
        uint32_t option;
        typename FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL cachePolicy;
    } testInputs[] = {
        {0, FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP},
        {2, FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WB},
        {3, FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WT},
        {4, FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WS}};

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
    args.setGeneralStateBaseAddress = true;

    auto &productHelper = getHelper<ProductHelper>();

    for (const auto &input : testInputs) {
        DebugManagerStateRestore restore;
        debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(input.option);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

        EXPECT_EQ(input.cachePolicy, sbaCmd.getL1CacheControlCachePolicy());

        debugManager.flags.ForceAllResourcesUncached.set(true);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_UC, sbaCmd.getL1CacheControlCachePolicy());
    }
}

HWTEST2_F(SbaTest, givenDebugFlagSetWhenAppendingRssThenProgramCorrectL1CachePolicy, IsAtLeastXeCore) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    size_t allocationSize = MemoryConstants::pageSize;
    AllocationProperties properties(pDevice->getRootDeviceIndex(), allocationSize, AllocationType::buffer, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    auto rssCmd = FamilyType::cmdInitRenderSurfaceState;

    auto multiGraphicsAllocation = MultiGraphicsAllocation(pDevice->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(allocation);

    EncodeSurfaceStateArgs args;
    args.outMemory = &rssCmd;
    args.graphicsAddress = allocation->getGpuAddress();
    args.size = allocation->getUnderlyingBufferSize();
    args.mocs = 0;
    args.numAvailableDevices = pDevice->getNumGenericSubDevices();
    args.allocation = allocation;
    args.gmmHelper = pDevice->getGmmHelper();
    args.areMultipleSubDevicesInContext = true;

    struct {
        uint32_t option;
        typename FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL cachePolicy;
    } testInputs[] = {
        {0, FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP},
        {2, FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB},
        {3, FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WT},
        {4, FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WS}};

    for (const auto &input : testInputs) {
        DebugManagerStateRestore restore;
        debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(input.option);
        EncodeSurfaceState<FamilyType>::encodeBuffer(args);
        EXPECT_EQ(input.cachePolicy, rssCmd.getL1CacheControlCachePolicy());
    }
    memoryManager->freeGraphicsMemory(allocation);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, SbaTest, whenGeneralStateBaseAddressIsProgrammedThenDecanonizedAddressIsWritten) {
    constexpr uint64_t generalStateBaseAddress = 0xffff800400010000ull;

    auto gmmHelper = pDevice->getGmmHelper();

    typename FamilyType::STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, gmmHelper, &ssh, &dsh, &ioh);
    args.generalStateBaseAddress = generalStateBaseAddress;
    args.instructionHeapBaseAddress = generalStateBaseAddress;
    args.setInstructionStateBaseAddress = true;
    args.setGeneralStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_NE(generalStateBaseAddress, sbaCmd.getGeneralStateBaseAddress());
    EXPECT_EQ(gmmHelper->decanonize(generalStateBaseAddress), sbaCmd.getGeneralStateBaseAddress());
}

HWTEST_F(SbaTest, givenNonZeroGeneralStateBaseAddressWhenProgrammingIsDisabledThenExpectCommandValueZero) {
    constexpr uint64_t generalStateBaseAddress = 0x80010000ull;

    typename FamilyType::STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getGmmHelper(), &ssh, &dsh, &ioh);
    args.generalStateBaseAddress = generalStateBaseAddress;
    args.instructionHeapBaseAddress = generalStateBaseAddress;
    args.setInstructionStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_EQ(0ull, sbaCmd.getGeneralStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getGeneralStateBufferSize());
    EXPECT_FALSE(sbaCmd.getGeneralStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getGeneralStateBufferSizeModifyEnable());
}

HWTEST_F(SbaTest, givenNonZeroInternalHeapBaseAddressWhenProgrammingIsDisabledThenExpectCommandValueZero) {
    constexpr uint64_t internalHeapBaseAddress = 0x80010000ull;

    typename FamilyType::STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getGmmHelper(), &ssh, &dsh, &ioh);
    args.generalStateBaseAddress = internalHeapBaseAddress;
    args.indirectObjectHeapBaseAddress = internalHeapBaseAddress;
    args.setGeneralStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_FALSE(sbaCmd.getInstructionBaseAddressModifyEnable());
    EXPECT_EQ(0ull, sbaCmd.getInstructionBaseAddress());
    EXPECT_FALSE(sbaCmd.getInstructionBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getInstructionBufferSize());
    EXPECT_EQ(0u, sbaCmd.getInstructionMemoryObjectControlState());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, SbaTest, givenSbaProgrammingWhenHeapsAreNotProvidedThenDontProgram) {
    auto gmmHelper = pDevice->getGmmHelper();

    constexpr uint64_t internalHeapBase = 0x10000;
    constexpr uint64_t instructionHeapBase = 0x10000;
    constexpr uint64_t generalStateBase = 0x30000;

    typename FamilyType::STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getGmmHelper());
    args.generalStateBaseAddress = generalStateBase;
    args.indirectObjectHeapBaseAddress = internalHeapBase;
    args.instructionHeapBaseAddress = instructionHeapBase;
    args.setGeneralStateBaseAddress = true;
    args.setInstructionStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_FALSE(sbaCmd.getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBufferSize());

    EXPECT_FALSE(sbaCmd.getIndirectObjectBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getIndirectObjectBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getIndirectObjectBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getIndirectObjectBufferSize());

    EXPECT_FALSE(sbaCmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd.getInstructionBaseAddressModifyEnable());
    EXPECT_EQ(instructionHeapBase, sbaCmd.getInstructionBaseAddress());
    EXPECT_TRUE(sbaCmd.getInstructionBufferSizeModifyEnable());
    EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, sbaCmd.getInstructionBufferSize());

    EXPECT_TRUE(sbaCmd.getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd.getGeneralStateBufferSizeModifyEnable());

    EXPECT_EQ(gmmHelper->decanonize(generalStateBase), sbaCmd.getGeneralStateBaseAddress());
    EXPECT_EQ(0xfffffu, sbaCmd.getGeneralStateBufferSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, SbaTest,
            givenNoHeapsProvidedWhenSBAIsProgrammedThenBaseAddressesAreNotSetAndBindlessSurfaceStateSizeSetToMax) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto gmmHelper = pDevice->getGmmHelper();

    constexpr uint64_t instructionHeapBase = 0x10000;
    constexpr uint64_t internalHeapBase = 0x10000;
    constexpr uint64_t generalStateBase = 0x30000;

    STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, gmmHelper);
    args.generalStateBaseAddress = generalStateBase;
    args.indirectObjectHeapBaseAddress = internalHeapBase;
    args.instructionHeapBaseAddress = instructionHeapBase;
    args.setGeneralStateBaseAddress = true;
    args.setInstructionStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_FALSE(sbaCmd.getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBufferSize());

    EXPECT_FALSE(sbaCmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd.getInstructionBaseAddressModifyEnable());
    EXPECT_EQ(instructionHeapBase, sbaCmd.getInstructionBaseAddress());
    EXPECT_TRUE(sbaCmd.getInstructionBufferSizeModifyEnable());
    EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, sbaCmd.getInstructionBufferSize());

    EXPECT_TRUE(sbaCmd.getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd.getGeneralStateBufferSizeModifyEnable());
    if constexpr (is64bit) {
        EXPECT_EQ(gmmHelper->decanonize(internalHeapBase), sbaCmd.getGeneralStateBaseAddress());
    } else {
        EXPECT_EQ(generalStateBase, sbaCmd.getGeneralStateBaseAddress());
    }
    EXPECT_EQ(0xfffffu, sbaCmd.getGeneralStateBufferSize());

    EXPECT_EQ(0u, sbaCmd.getBindlessSurfaceStateBaseAddress());
    EXPECT_FALSE(sbaCmd.getBindlessSurfaceStateBaseAddressModifyEnable());
    uint32_t defaultBindlessSurfaceStateSize = StateBaseAddressHelper<FamilyType>::getMaxBindlessSurfaceStates();
    EXPECT_EQ(defaultBindlessSurfaceStateSize, sbaCmd.getBindlessSurfaceStateSize());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, SbaTest,
            givenNoHeapsProvidedAndBindlessBaseSetWhenSBAIsProgrammedThenBindlessSurfaceStateSizeSetToZeroAndBaseAddressSetToPassedValue) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto gmmHelper = pDevice->getGmmHelper();

    constexpr uint64_t instructionHeapBase = 0x10000;
    constexpr uint64_t internalHeapBase = 0x10000;
    constexpr uint64_t generalStateBase = 0x30000;

    STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, gmmHelper);
    args.generalStateBaseAddress = generalStateBase;
    args.indirectObjectHeapBaseAddress = internalHeapBase;
    args.instructionHeapBaseAddress = instructionHeapBase;
    args.setGeneralStateBaseAddress = true;
    args.setInstructionStateBaseAddress = true;
    args.bindlessSurfaceStateBaseAddress = 0x90004000;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_FALSE(sbaCmd.getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBufferSize());

    EXPECT_FALSE(sbaCmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getSurfaceStateBaseAddress());

    EXPECT_TRUE(sbaCmd.getInstructionBaseAddressModifyEnable());
    EXPECT_EQ(instructionHeapBase, sbaCmd.getInstructionBaseAddress());
    EXPECT_TRUE(sbaCmd.getInstructionBufferSizeModifyEnable());
    EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, sbaCmd.getInstructionBufferSize());

    EXPECT_TRUE(sbaCmd.getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd.getGeneralStateBufferSizeModifyEnable());
    if constexpr (is64bit) {
        EXPECT_EQ(gmmHelper->decanonize(internalHeapBase), sbaCmd.getGeneralStateBaseAddress());
    } else {
        EXPECT_EQ(generalStateBase, sbaCmd.getGeneralStateBaseAddress());
    }
    EXPECT_EQ(0xfffffu, sbaCmd.getGeneralStateBufferSize());

    auto surfaceStateCount = StateBaseAddressHelper<FamilyType>::getMaxBindlessSurfaceStates();
    EXPECT_EQ(0x90004000u, sbaCmd.getBindlessSurfaceStateBaseAddress());
    EXPECT_TRUE(sbaCmd.getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(surfaceStateCount, sbaCmd.getBindlessSurfaceStateSize());
}

HWTEST2_F(SbaTest, GivenPlatformNotSupportingIndirectHeapBaseWhenProgramIndirectHeapThenNothingHappens, IsAtLeastXeCore) {
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(nullptr, nullptr);
    StateBaseAddressHelper<FamilyType>::appendIohParameters(args);
}

HWTEST_F(SbaTest, givenStateBaseAddressPropertiesWhenSettingDynamicStateSurfaceStateMocsPropertiesThenCommandDispatchedCorrectly) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore restore;
    debugManager.flags.ForceL1Caching.set(0);

    constexpr uint64_t surfaceHeapBase = 0x10000;
    constexpr uint64_t dynamicHeapBase = 0x20000;
    constexpr uint32_t surfaceHeapSize = 0x10;
    constexpr uint32_t dynamicHeapSize = 0x20;
    constexpr uint32_t mocsIndex = 0x8;

    auto gmmHelper = pDevice->getGmmHelper();
    StateBaseAddressProperties sbaProperties;
    sbaProperties.initSupport(pDevice->getRootDeviceEnvironment());

    STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, gmmHelper, &sbaProperties);

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_FALSE(sbaCmd.getDynamicStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getDynamicStateBufferSize());

    EXPECT_FALSE(sbaCmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getSurfaceStateBaseAddress());

    EXPECT_EQ(0u, sbaCmd.getStatelessDataPortAccessMemoryObjectControlState());

    sbaProperties.setPropertiesBindingTableSurfaceState(surfaceHeapBase, surfaceHeapSize, surfaceHeapBase, surfaceHeapSize);
    sbaProperties.setPropertiesDynamicState(dynamicHeapBase, dynamicHeapSize);
    sbaProperties.setPropertyStatelessMocs(mocsIndex);

    sbaCmd = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_TRUE(sbaCmd.getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd.getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(dynamicHeapBase, sbaCmd.getDynamicStateBaseAddress());
    EXPECT_EQ(dynamicHeapSize, sbaCmd.getDynamicStateBufferSize());

    EXPECT_TRUE(sbaCmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(surfaceHeapBase, sbaCmd.getSurfaceStateBaseAddress());

    EXPECT_EQ((mocsIndex << 1), sbaCmd.getStatelessDataPortAccessMemoryObjectControlState());
}

using IndirectBaseAddressPlatforms = IsGen12LP;

HWTEST2_F(SbaTest, givenStateBaseAddressPropertiesWhenSettingIndirectStatePropertyThenCommandDispatchedCorrectlyIndirectBaseAddress, IndirectBaseAddressPlatforms) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    constexpr uint64_t indirectHeapBase = 0x10000;
    constexpr uint32_t indirectHeapSize = 0x10;

    auto gmmHelper = pDevice->getGmmHelper();
    StateBaseAddressProperties sbaProperties;

    STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, gmmHelper, &sbaProperties);

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_FALSE(sbaCmd.getIndirectObjectBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getIndirectObjectBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getIndirectObjectBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getIndirectObjectBufferSize());

    sbaProperties.setPropertiesIndirectState(indirectHeapBase, indirectHeapSize);

    sbaCmd = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_TRUE(sbaCmd.getIndirectObjectBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd.getIndirectObjectBufferSizeModifyEnable());
    EXPECT_EQ(indirectHeapBase, sbaCmd.getIndirectObjectBaseAddress());
    EXPECT_EQ(indirectHeapSize, sbaCmd.getIndirectObjectBufferSize());
}

using GlobalBaseAddressPlatforms = IsAtLeastXeCore;

using BindlessSurfaceAddressPlatforms = MatchAny;

HWTEST2_F(SbaTest, givenStateBaseAddressPropertiesWhenSettingBindlessSurfaceStatePropertyThenCommandDispatchedCorrectlyBindlessBaseAddress, BindlessSurfaceAddressPlatforms) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    constexpr uint64_t surfaceHeapBase = 0x10000;
    constexpr uint32_t surfaceHeapSize = 0x10;

    uint32_t defaultBindlessSurfaceStateSize = StateBaseAddressHelper<FamilyType>::getMaxBindlessSurfaceStates();

    auto gmmHelper = pDevice->getGmmHelper();
    StateBaseAddressProperties sbaProperties;
    sbaProperties.initSupport(pDevice->getRootDeviceEnvironment());

    STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, gmmHelper, &sbaProperties);

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);
    EXPECT_EQ(defaultBindlessSurfaceStateSize, sbaCmd.getBindlessSurfaceStateSize());
    EXPECT_EQ(0u, sbaCmd.getBindlessSurfaceStateBaseAddress());
    EXPECT_FALSE(sbaCmd.getBindlessSurfaceStateBaseAddressModifyEnable());

    sbaCmd = FamilyType::cmdInitStateBaseAddress;
    args.bindlessSurfaceStateBaseAddress = 0x80004000;
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_EQ(defaultBindlessSurfaceStateSize, sbaCmd.getBindlessSurfaceStateSize());
    EXPECT_EQ(0x80004000u, sbaCmd.getBindlessSurfaceStateBaseAddress());
    EXPECT_TRUE(sbaCmd.getBindlessSurfaceStateBaseAddressModifyEnable());

    sbaProperties.setPropertiesBindingTableSurfaceState(surfaceHeapBase, surfaceHeapSize, surfaceHeapBase, surfaceHeapSize);

    sbaCmd = FamilyType::cmdInitStateBaseAddress;
    args.bindlessSurfaceStateBaseAddress = 0;
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_EQ(surfaceHeapSize * MemoryConstants::pageSize / sizeof(RENDER_SURFACE_STATE), sbaCmd.getBindlessSurfaceStateSize());
    EXPECT_EQ(surfaceHeapBase, sbaCmd.getBindlessSurfaceStateBaseAddress());
    EXPECT_TRUE(sbaCmd.getBindlessSurfaceStateBaseAddressModifyEnable());
}

TEST(SbaHelperTest, givenIndirectHeapWhenGetStateBaseAddressAndGetStateSizeThenReturnValuesBasedOnGlobalHeapsPresence) {
    auto cpuBaseAddress = reinterpret_cast<void *>(0x3000);
    MockGraphicsAllocation graphicsAllocation(cpuBaseAddress, 4096u);
    graphicsAllocation.setGpuBaseAddress(4096u);

    IndirectHeap heap(&graphicsAllocation, false);

    auto gpuAddress = heap.getGraphicsAllocation()->getGpuAddress();
    auto gpuBaseAddress = heap.getGraphicsAllocation()->getGpuBaseAddress();
    auto heapSize = (static_cast<uint32_t>(heap.getMaxAvailableSpace()) + MemoryConstants::pageMask) / MemoryConstants::pageSize;

    bool useGlobalHeaps = false;

    {
        useGlobalHeaps = false;
        EXPECT_EQ(gpuAddress, NEO::getStateBaseAddress(heap, useGlobalHeaps));
        EXPECT_EQ(heapSize, NEO::getStateSize(heap, useGlobalHeaps));
    }
    {
        useGlobalHeaps = true;
        EXPECT_EQ(gpuBaseAddress, NEO::getStateBaseAddress(heap, useGlobalHeaps));
        EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, NEO::getStateSize(heap, useGlobalHeaps));
    }

    {
        useGlobalHeaps = true;
        EXPECT_EQ(gpuAddress, NEO::getStateBaseAddressForSsh(heap, useGlobalHeaps));
        EXPECT_EQ(heapSize, NEO::getStateSizeForSsh(heap, useGlobalHeaps));
    }
    {
        useGlobalHeaps = false;
        EXPECT_EQ(gpuAddress, NEO::getStateBaseAddressForSsh(heap, useGlobalHeaps));
        EXPECT_EQ(heapSize, NEO::getStateSizeForSsh(heap, useGlobalHeaps));
    }
}
