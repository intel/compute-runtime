/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/state_base_address_tests.h"

#include "shared/source/command_container/command_encoder.h"
#include "shared/test/common/test_macros/hw_test.h"

using IsBetweenSklAndTgllp = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

HWTEST2_F(SBATest, WhenAppendStateBaseAddressParametersIsCalledThenSBACmdHasBindingSurfaceStateProgrammed, IsBetweenSklAndTgllp) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    STATE_BASE_ADDRESS stateBaseAddress;
    stateBaseAddress.setBindlessSurfaceStateSize(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddress(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddressModifyEnable(false);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(
        &stateBaseAddress,
        &ssh,
        false,
        0,
        nullptr,
        false,
        MemoryCompressionState::NotApplicable,
        true,
        false,
        1u);

    EXPECT_EQ(ssh.getMaxAvailableSpace() / 64 - 1, stateBaseAddress.getBindlessSurfaceStateSize());
    EXPECT_EQ(ssh.getHeapGpuBase(), stateBaseAddress.getBindlessSurfaceStateBaseAddress());
    EXPECT_TRUE(stateBaseAddress.getBindlessSurfaceStateBaseAddressModifyEnable());
}

HWTEST2_F(SBATest, WhenProgramStateBaseAddressParametersIsCalledThenSBACmdHasBindingSurfaceStateProgrammed, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    STATE_BASE_ADDRESS stateBaseAddress;
    stateBaseAddress.setBindlessSurfaceStateSize(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddress(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddressModifyEnable(false);

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(commandStream.getSpace(0));
    *cmd = stateBaseAddress;

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(
        cmd,
        nullptr,
        nullptr,
        &ssh,
        0,
        false,
        0,
        0,
        0,
        0,
        false,
        false,
        pDevice->getGmmHelper(),
        true,
        MemoryCompressionState::NotApplicable,
        false,
        1u,
        nullptr);

    EXPECT_EQ(ssh.getMaxAvailableSpace() / 64 - 1, cmd->getBindlessSurfaceStateSize());
    EXPECT_EQ(ssh.getHeapGpuBase(), cmd->getBindlessSurfaceStateBaseAddress());
    EXPECT_TRUE(cmd->getBindlessSurfaceStateBaseAddressModifyEnable());
}

using SbaForBindlessTests = Test<DeviceFixture>;

HWTEST2_F(SbaForBindlessTests, givenGlobalBindlessBaseAddressWhenProgramStateBaseAddressThenSbaProgrammedCorrectly, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    uint64_t globalBindlessHeapsBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(
        cmd,
        nullptr,
        nullptr,
        nullptr,
        0,
        false,
        0,
        0,
        0,
        globalBindlessHeapsBaseAddress,
        false,
        true,
        pDevice->getGmmHelper(),
        true,
        MemoryCompressionState::NotApplicable,
        false,
        1u,
        nullptr);
    EXPECT_TRUE(cmd->getBindlessSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(cmd->getBindlessSurfaceStateBaseAddress(), globalBindlessHeapsBaseAddress);

    auto surfaceStateCount = StateBaseAddressHelper<FamilyType>::getMaxBindlessSurfaceStates();
    EXPECT_EQ(surfaceStateCount, cmd->getBindlessSurfaceStateSize());
}

using IohSupported = IsWithinGfxCore<GFXCORE_FAMILY::IGFX_GEN9_CORE, GFXCORE_FAMILY::IGFX_GEN12LP_CORE>;

HWTEST2_F(SbaForBindlessTests, givenGlobalBindlessBaseAddressWhenPassingIndirectBaseAddressThenIndirectBaseAddressIsSet, IohSupported) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    uint64_t globalBindlessHeapsBaseAddress = 0x12340000;
    uint64_t indirectObjectBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(
        cmd,
        nullptr,
        nullptr,
        nullptr,
        0,
        false,
        0,
        indirectObjectBaseAddress,
        0,
        globalBindlessHeapsBaseAddress,
        false,
        true,
        pDevice->getGmmHelper(),
        true,
        MemoryCompressionState::NotApplicable,
        false,
        1u,
        nullptr);

    EXPECT_EQ(cmd->getIndirectObjectBaseAddress(), indirectObjectBaseAddress);
}

HWTEST2_F(SBATest, givenSbaWhenOverrideBindlessSurfaceBaseIsFalseThenBindlessSurfaceBaseIsNotSet, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    STATE_BASE_ADDRESS stateBaseAddress;
    stateBaseAddress.setBindlessSurfaceStateSize(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddress(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddressModifyEnable(false);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(
        &stateBaseAddress,
        &ssh,
        false,
        0,
        pDevice->getRootDeviceEnvironment().getGmmHelper(),
        false,
        MemoryCompressionState::NotApplicable,
        false,
        false,
        1u);

    EXPECT_EQ(0u, stateBaseAddress.getBindlessSurfaceStateBaseAddress());
}

HWTEST2_F(SBATest, givenGlobalBindlessBaseAddressWhenSshIsPassedThenBindlessSurfaceBaseIsGlobalHeapBase, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    uint64_t globalBindlessHeapsBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(
        cmd,
        nullptr,
        nullptr,
        &ssh,
        0,
        false,
        0,
        0,
        0,
        globalBindlessHeapsBaseAddress,
        false,
        true,
        pDevice->getGmmHelper(),
        true,
        MemoryCompressionState::NotApplicable,
        false,
        1u,
        nullptr);
    EXPECT_EQ(cmd->getBindlessSurfaceStateBaseAddress(), globalBindlessHeapsBaseAddress);
}
HWTEST2_F(SBATest, givenSurfaceStateHeapWhenNotUsingGlobalHeapBaseThenBindlessSurfaceBaseIsSshBase, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    uint64_t globalBindlessHeapsBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(
        cmd,
        nullptr,
        nullptr,
        &ssh,
        0,
        false,
        0,
        0,
        0,
        globalBindlessHeapsBaseAddress,
        false,
        false,
        pDevice->getGmmHelper(),
        true,
        MemoryCompressionState::NotApplicable,
        false,
        1u,
        nullptr);
    EXPECT_EQ(ssh.getHeapGpuBase(), cmd->getBindlessSurfaceStateBaseAddress());
}

HWTEST2_F(SBATest, givenStateBaseAddressAndDebugFlagSetWhenAppendExtraCacheSettingsThenNothingChanged, IsAtMostXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto stateBaseAddress = FamilyType::cmdInitStateBaseAddress;
    auto expectedStateBaseAddress = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(&stateBaseAddress, &hardwareInfo);
    EXPECT_EQ(0, memcmp(&stateBaseAddress, &expectedStateBaseAddress, sizeof(STATE_BASE_ADDRESS)));

    DebugManager.flags.ForceStatelessL1CachingPolicy.set(2);
    StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(&stateBaseAddress, &hardwareInfo);
    EXPECT_EQ(0, memcmp(&stateBaseAddress, &expectedStateBaseAddress, sizeof(STATE_BASE_ADDRESS)));
}

HWTEST2_F(SBATest, givenDebugFlagSetWhenAppendingSbaThenProgramCorrectL1CachePolicy, IsAtLeastXeHpgCore) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    AllocationProperties properties(pDevice->getRootDeviceIndex(), 1, AllocationType::BUFFER, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    IndirectHeap indirectHeap(allocation, 1);
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;

    struct {
        uint32_t option;
        typename FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY cachePolicy;
    } testInputs[] = {
        {0, FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP},
        {2, FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB},
        {3, FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WT},
        {4, FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WS}};

    for (const auto &input : testInputs) {
        DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(input.option);
        StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(&sbaCmd, &indirectHeap, true, 0,
                                                                             pDevice->getRootDeviceEnvironment().getGmmHelper(), false,
                                                                             MemoryCompressionState::NotApplicable, true, false, 1u);

        EXPECT_EQ(input.cachePolicy, sbaCmd.getL1CachePolicyL1CacheControl());
    }
    memoryManager->freeGraphicsMemory(allocation);
}

HWTEST2_F(SBATest, givenDebugFlagSetWhenAppendingRssThenProgramCorrectL1CachePolicy, IsAtLeastXeHpgCore) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    size_t allocationSize = MemoryConstants::pageSize;
    AllocationProperties properties(pDevice->getRootDeviceIndex(), allocationSize, AllocationType::BUFFER, pDevice->getDeviceBitfield());
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
        DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(input.option);
        EncodeSurfaceState<FamilyType>::encodeBuffer(args);
        EXPECT_EQ(input.cachePolicy, rssCmd.getL1CachePolicyL1CacheControl());
    }
    memoryManager->freeGraphicsMemory(allocation);
}