/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/state_base_address_tests.h"

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "encode_surface_state_args.h"

using IsBetweenSklAndTgllp = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

HWTEST2_F(SbaTest, WhenAppendStateBaseAddressParametersIsCalledThenSBACmdHasBindingSurfaceStateProgrammed, IsBetweenSklAndTgllp) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

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

HWTEST2_F(SbaTest, WhenProgramStateBaseAddressParametersIsCalledThenSBACmdHasBindingSurfaceStateProgrammed, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

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
          givenProgramSurfaceStateBaseAddressUsingHeapBaseWhenOverrideSurfaceStateBaseAddressUsedThenSbaDispatchedWithOverrideValue, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

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

HWTEST2_F(SbaForBindlessTests, givenGlobalBindlessBaseAddressWhenProgramStateBaseAddressThenSbaProgrammedCorrectly, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    constexpr uint64_t globalBindlessHeapsBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(cmd, pDevice->getGmmHelper());
    args.globalHeapsBaseAddress = globalBindlessHeapsBaseAddress;
    args.useGlobalHeapsBaseAddress = true;

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
    EXPECT_EQ(globalBindlessHeapsBaseAddress, cmd->getSurfaceStateBaseAddress());
}

HWTEST2_F(SbaForBindlessTests,
          givenGlobalBindlessBaseAddressOverridenSurfaceStateBaseAddressWhenProgramStateBaseAddressThenSbaProgrammedCorrectly, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

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

using IohSupported = IsWithinGfxCore<GFXCORE_FAMILY::IGFX_GEN9_CORE, GFXCORE_FAMILY::IGFX_GEN12LP_CORE>;

HWTEST2_F(SbaForBindlessTests, givenGlobalBindlessBaseAddressWhenPassingIndirectBaseAddressThenIndirectBaseAddressIsSet, IohSupported) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

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

HWTEST2_F(SbaTest, givenSbaWhenOverrideBindlessSurfaceBaseIsFalseThenBindlessSurfaceBaseIsNotSet, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    STATE_BASE_ADDRESS stateBaseAddress;
    stateBaseAddress.setBindlessSurfaceStateSize(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddress(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddressModifyEnable(false);

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&stateBaseAddress, pDevice->getRootDeviceEnvironment().getGmmHelper());
    args.useGlobalHeapsBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(0u, stateBaseAddress.getBindlessSurfaceStateBaseAddress());
}

HWTEST2_F(SbaTest, givenGlobalBindlessBaseAddressWhenSshIsPassedThenBindlessSurfaceBaseIsGlobalHeapBase, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

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
HWTEST2_F(SbaTest, givenSurfaceStateHeapWhenNotUsingGlobalHeapBaseThenBindlessSurfaceBaseIsSshBase, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    constexpr uint64_t globalBindlessHeapsBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(cmd, pDevice->getGmmHelper(), &ssh, nullptr, nullptr);
    args.globalHeapsBaseAddress = globalBindlessHeapsBaseAddress;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_EQ(ssh.getHeapGpuBase(), cmd->getBindlessSurfaceStateBaseAddress());
}

HWTEST2_F(SbaTest, givenNotUsedGlobalHeapBaseAndSshPassedWhenBindlessSurfStateBaseIsPassedThenBindlessSurfaceBaseIsSetToPassedValue, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

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

HWTEST2_F(SbaTest, givenStateBaseAddressAndDebugFlagSetWhenAppendExtraCacheSettingsThenNothingChanged, IsAtMostXeHpCore) {
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

HWTEST2_F(SbaTest, givenStateBaseAddressAndDebugFlagSetWhenAppendExtraCacheSettingsThenProgramCorrectL1CachePolicy, IsAtLeastXeHpgCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto stateBaseAddress = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&stateBaseAddress, pDevice->getGmmHelper(), &ssh, nullptr, nullptr);
    auto &productHelper = getHelper<ProductHelper>();
    {
        DebugManagerStateRestore restore;
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, stateBaseAddress.getL1CachePolicyL1CacheControl());

        debugManager.flags.ForceStatelessL1CachingPolicy.set(2);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, stateBaseAddress.getL1CachePolicyL1CacheControl());

        debugManager.flags.ForceStatelessL1CachingPolicy.set(3);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WT, stateBaseAddress.getL1CachePolicyL1CacheControl());

        debugManager.flags.ForceStatelessL1CachingPolicy.set(4);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WS, stateBaseAddress.getL1CachePolicyL1CacheControl());

        debugManager.flags.ForceAllResourcesUncached.set(true);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_UC, stateBaseAddress.getL1CachePolicyL1CacheControl());
    }
    args.isDebuggerActive = true;
    {
        DebugManagerStateRestore restore;
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, stateBaseAddress.getL1CachePolicyL1CacheControl());

        debugManager.flags.ForceStatelessL1CachingPolicy.set(2);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, stateBaseAddress.getL1CachePolicyL1CacheControl());

        debugManager.flags.ForceStatelessL1CachingPolicy.set(3);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WT, stateBaseAddress.getL1CachePolicyL1CacheControl());

        debugManager.flags.ForceStatelessL1CachingPolicy.set(4);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WS, stateBaseAddress.getL1CachePolicyL1CacheControl());

        debugManager.flags.ForceAllResourcesUncached.set(true);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_UC, stateBaseAddress.getL1CachePolicyL1CacheControl());
    }
}

HWTEST2_F(SbaTest, givenDebugFlagSetWhenAppendingSbaThenProgramCorrectL1CachePolicy, IsAtLeastXeHpgCore) {
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;

    struct {
        uint32_t option;
        typename FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY cachePolicy;
    } testInputs[] = {
        {0, FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP},
        {2, FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB},
        {3, FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WT},
        {4, FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WS}};

    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
    args.setGeneralStateBaseAddress = true;

    auto &productHelper = getHelper<ProductHelper>();

    for (const auto &input : testInputs) {
        DebugManagerStateRestore restore;
        debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(input.option);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

        EXPECT_EQ(input.cachePolicy, sbaCmd.getL1CachePolicyL1CacheControl());

        debugManager.flags.ForceAllResourcesUncached.set(true);
        updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

        StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_UC, sbaCmd.getL1CachePolicyL1CacheControl());
    }
}

HWTEST2_F(SbaTest, givenDebugFlagSetWhenAppendingRssThenProgramCorrectL1CachePolicy, IsAtLeastXeHpgCore) {
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
        typename FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY cachePolicy;
    } testInputs[] = {
        {0, FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WBP},
        {2, FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB},
        {3, FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WT},
        {4, FamilyType::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WS}};

    for (const auto &input : testInputs) {
        DebugManagerStateRestore restore;
        debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(input.option);
        EncodeSurfaceState<FamilyType>::encodeBuffer(args);
        EXPECT_EQ(input.cachePolicy, rssCmd.getL1CachePolicyL1CacheControl());
    }
    memoryManager->freeGraphicsMemory(allocation);
}

HWCMDTEST_F(IGFX_GEN8_CORE, SbaTest, whenGeneralStateBaseAddressIsProgrammedThenDecanonizedAddressIsWritten) {
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

HWCMDTEST_F(IGFX_GEN8_CORE, SbaTest, givenSbaProgrammingWhenHeapsAreNotProvidedThenDontProgram) {
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

HWTEST2_F(SbaTest, GivenPlatformNotSupportingIndirectHeapBaseWhenProgramIndirectHeapThenNothingHappens, IsAtLeastXeHpCore) {
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

using IndirectBaseAddressPlatforms = IsAtMostGen12lp;

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

using GlobalBaseAddressPlatforms = IsAtLeastXeHpCore;

HWTEST2_F(SbaTest, givenStateBaseAddressPropertiesWhenSettingIndirectStateAndGlobalAtomicsPropertyThenCommandDispatchedCorrectlyGlobalBaseAddressAndGlobalAtomics, GlobalBaseAddressPlatforms) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    constexpr uint64_t indirectHeapBase = 0x10000;
    constexpr uint32_t indirectHeapSize = 0x10;
    constexpr uint32_t constGlobalHeapSize = 0xfffff;

    auto gmmHelper = pDevice->getGmmHelper();
    StateBaseAddressProperties sbaProperties;

    STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, gmmHelper, &sbaProperties);
    args.isMultiOsContextCapable = true;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_FALSE(sbaCmd.getGeneralStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getGeneralStateBufferSizeModifyEnable());
    EXPECT_EQ(0u, sbaCmd.getGeneralStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getGeneralStateBufferSize());

    EXPECT_TRUE(sbaCmd.getDisableSupportForMultiGpuAtomicsForStatelessAccesses());

    sbaProperties.setPropertiesIndirectState(indirectHeapBase, indirectHeapSize);
    sbaProperties.globalAtomics.set(1);

    sbaCmd = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_TRUE(sbaCmd.getGeneralStateBaseAddressModifyEnable());
    EXPECT_TRUE(sbaCmd.getGeneralStateBufferSizeModifyEnable());
    EXPECT_EQ(indirectHeapBase, sbaCmd.getGeneralStateBaseAddress());
    EXPECT_EQ(constGlobalHeapSize, sbaCmd.getGeneralStateBufferSize());

    EXPECT_FALSE(sbaCmd.getDisableSupportForMultiGpuAtomicsForStatelessAccesses());

    sbaProperties.globalAtomics.set(0);

    sbaCmd = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_TRUE(sbaCmd.getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
}

using BindlessSurfaceAddressPlatforms = IsAtLeastGen9;

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
