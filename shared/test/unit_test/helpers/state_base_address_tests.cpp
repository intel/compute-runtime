/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/state_base_address_tests.h"

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

using IsBetweenSklAndTgllp = IsWithinProducts<IGFX_SKYLAKE, IGFX_TIGERLAKE_LP>;

HWTEST2_F(SbaTest, WhenAppendStateBaseAddressParametersIsCalledThenSBACmdHasBindingSurfaceStateProgrammed, IsBetweenSklAndTgllp) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    STATE_BASE_ADDRESS stateBaseAddress;
    stateBaseAddress.setBindlessSurfaceStateSize(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddress(0);
    stateBaseAddress.setBindlessSurfaceStateBaseAddressModifyEnable(false);

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        &stateBaseAddress,                     // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        &ssh,                                  // ssh
        nullptr,                               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);

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

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        cmd,                                   // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        &ssh,                                  // ssh
        pDevice->getGmmHelper(),               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };

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

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        surfaceStateBaseAddress,               // surfaceStateBaseAddress
        &cmd,                                  // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        &ssh,                                  // ssh
        pDevice->getGmmHelper(),               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        true,                                  // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_TRUE(cmd.getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(surfaceStateBaseAddress, cmd.getSurfaceStateBaseAddress());
}

using SbaForBindlessTests = Test<DeviceFixture>;

HWTEST2_F(SbaForBindlessTests, givenGlobalBindlessBaseAddressWhenProgramStateBaseAddressThenSbaProgrammedCorrectly, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    constexpr uint64_t globalBindlessHeapsBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        globalBindlessHeapsBaseAddress,        // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        cmd,                                   // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        nullptr,                               // ssh
        pDevice->getGmmHelper(),               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        true,                                  // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };

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

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        globalBindlessHeapsBaseAddress,        // globalHeapsBaseAddress
        surfaceStateBaseAddress,               // surfaceStateBaseAddress
        cmd,                                   // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        nullptr,                               // ssh
        pDevice->getGmmHelper(),               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        true,                                  // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        true,                                  // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };

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

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        indirectObjectBaseAddress,             // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        globalBindlessHeapsBaseAddress,        // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        cmd,                                   // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        nullptr,                               // ssh
        pDevice->getGmmHelper(),               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        true,                                  // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };

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

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                                  // generalStateBase
        0,                                                  // indirectObjectHeapBaseAddress
        0,                                                  // instructionHeapBaseAddress
        0,                                                  // globalHeapsBaseAddress
        0,                                                  // surfaceStateBaseAddress
        &stateBaseAddress,                                  // stateBaseAddressCmd
        nullptr,                                            // dsh
        nullptr,                                            // ioh
        nullptr,                                            // ssh
        pDevice->getRootDeviceEnvironment().getGmmHelper(), // gmmHelper
        nullptr,                                            // hwInfo
        0,                                                  // statelessMocsIndex
        MemoryCompressionState::NotApplicable,              // memoryCompressionState
        false,                                              // setInstructionStateBaseAddress
        false,                                              // setGeneralStateBaseAddress
        true,                                               // useGlobalHeapsBaseAddress
        false,                                              // isMultiOsContextCapable
        false,                                              // useGlobalAtomics
        false,                                              // areMultipleSubDevicesInContext
        false,                                              // overrideSurfaceStateBaseAddress
        false                                               // isDebuggerActive
    };

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, false);

    EXPECT_EQ(0u, stateBaseAddress.getBindlessSurfaceStateBaseAddress());
}

HWTEST2_F(SbaTest, givenGlobalBindlessBaseAddressWhenSshIsPassedThenBindlessSurfaceBaseIsGlobalHeapBase, IsAtLeastSkl) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    EXPECT_NE(IGFX_BROADWELL, ::productFamily);

    constexpr uint64_t globalBindlessHeapsBaseAddress = 0x12340000;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    STATE_BASE_ADDRESS *cmd = reinterpret_cast<STATE_BASE_ADDRESS *>(cmdStream.getSpace(0));

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        globalBindlessHeapsBaseAddress,        // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        cmd,                                   // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        &ssh,                                  // ssh
        pDevice->getGmmHelper(),               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        true,                                  // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };

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

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        globalBindlessHeapsBaseAddress,        // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        cmd,                                   // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        &ssh,                                  // ssh
        pDevice->getGmmHelper(),               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };

    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_EQ(ssh.getHeapGpuBase(), cmd->getBindlessSurfaceStateBaseAddress());
}

HWTEST2_F(SbaTest, givenStateBaseAddressAndDebugFlagSetWhenAppendExtraCacheSettingsThenNothingChanged, IsAtMostXeHpCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto stateBaseAddress = FamilyType::cmdInitStateBaseAddress;
    auto expectedStateBaseAddress = FamilyType::cmdInitStateBaseAddress;
    DebugManagerStateRestore restore;

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        &stateBaseAddress,                     // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        &ssh,                                  // ssh
        pDevice->getGmmHelper(),               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };

    StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
    EXPECT_EQ(0, memcmp(&stateBaseAddress, &expectedStateBaseAddress, sizeof(STATE_BASE_ADDRESS)));

    DebugManager.flags.ForceStatelessL1CachingPolicy.set(2);
    StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
    EXPECT_EQ(0, memcmp(&stateBaseAddress, &expectedStateBaseAddress, sizeof(STATE_BASE_ADDRESS)));

    DebugManager.flags.ForceAllResourcesUncached.set(true);
    StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
    EXPECT_EQ(0, memcmp(&stateBaseAddress, &expectedStateBaseAddress, sizeof(STATE_BASE_ADDRESS)));
}

HWTEST2_F(SbaTest, givenStateBaseAddressAndDebugFlagSetWhenAppendExtraCacheSettingsThenProgramCorrectL1CachePolicy, IsAtLeastXeHpgCore) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto stateBaseAddress = FamilyType::cmdInitStateBaseAddress;

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                     // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        &stateBaseAddress,                     // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        &ssh,                                  // ssh
        pDevice->getGmmHelper(),               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };
    {
        DebugManagerStateRestore restore;
        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, stateBaseAddress.getL1CachePolicyL1CacheControl());

        DebugManager.flags.ForceStatelessL1CachingPolicy.set(2);
        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, stateBaseAddress.getL1CachePolicyL1CacheControl());

        DebugManager.flags.ForceStatelessL1CachingPolicy.set(3);
        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WT, stateBaseAddress.getL1CachePolicyL1CacheControl());

        DebugManager.flags.ForceStatelessL1CachingPolicy.set(4);
        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WS, stateBaseAddress.getL1CachePolicyL1CacheControl());

        DebugManager.flags.ForceAllResourcesUncached.set(true);
        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_UC, stateBaseAddress.getL1CachePolicyL1CacheControl());
    }
    args.isDebuggerActive = true;
    {
        DebugManagerStateRestore restore;
        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, stateBaseAddress.getL1CachePolicyL1CacheControl());

        DebugManager.flags.ForceStatelessL1CachingPolicy.set(2);
        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, stateBaseAddress.getL1CachePolicyL1CacheControl());

        DebugManager.flags.ForceStatelessL1CachingPolicy.set(3);
        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WT, stateBaseAddress.getL1CachePolicyL1CacheControl());

        DebugManager.flags.ForceStatelessL1CachingPolicy.set(4);
        StateBaseAddressHelper<FamilyType>::appendExtraCacheSettings(args);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WS, stateBaseAddress.getL1CachePolicyL1CacheControl());

        DebugManager.flags.ForceAllResourcesUncached.set(true);
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

    StateBaseAddressHelperArgs<FamilyType> args = {
        0,                                                  // generalStateBase
        0,                                                  // indirectObjectHeapBaseAddress
        0,                                                  // instructionHeapBaseAddress
        0,                                                  // globalHeapsBaseAddress
        0,                                                  // surfaceStateBaseAddress
        &sbaCmd,                                            // stateBaseAddressCmd
        nullptr,                                            // dsh
        nullptr,                                            // ioh
        &ssh,                                               // ssh
        pDevice->getRootDeviceEnvironment().getGmmHelper(), // gmmHelper
        nullptr,                                            // hwInfo
        0,                                                  // statelessMocsIndex
        MemoryCompressionState::NotApplicable,              // memoryCompressionState
        false,                                              // setInstructionStateBaseAddress
        true,                                               // setGeneralStateBaseAddress
        false,                                              // useGlobalHeapsBaseAddress
        false,                                              // isMultiOsContextCapable
        false,                                              // useGlobalAtomics
        false,                                              // areMultipleSubDevicesInContext
        false,                                              // overrideSurfaceStateBaseAddress
        false                                               // isDebuggerActive
    };

    for (const auto &input : testInputs) {
        DebugManagerStateRestore restore;
        DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(input.option);
        StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);

        EXPECT_EQ(input.cachePolicy, sbaCmd.getL1CachePolicyL1CacheControl());

        DebugManager.flags.ForceAllResourcesUncached.set(true);
        StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);
        EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_UC, sbaCmd.getL1CachePolicyL1CacheControl());
    }
}

HWTEST2_F(SbaTest, givenDebugFlagSetWhenAppendingRssThenProgramCorrectL1CachePolicy, IsAtLeastXeHpgCore) {
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
        DebugManagerStateRestore restore;
        DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(input.option);
        EncodeSurfaceState<FamilyType>::encodeBuffer(args);
        EXPECT_EQ(input.cachePolicy, rssCmd.getL1CachePolicyL1CacheControl());
    }
    memoryManager->freeGraphicsMemory(allocation);
}

HWCMDTEST_F(IGFX_GEN8_CORE, SbaTest, whenGeneralStateBaseAddressIsProgrammedThenDecanonizedAddressIsWritten) {
    constexpr uint64_t generalStateBaseAddress = 0xffff800400010000ull;

    auto gmmHelper = pDevice->getGmmHelper();

    typename FamilyType::STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = {
        generalStateBaseAddress,               // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        generalStateBaseAddress,               // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        &sbaCmd,                               // stateBaseAddressCmd
        &dsh,                                  // dsh
        &ioh,                                  // ioh
        &ssh,                                  // ssh
        gmmHelper,                             // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        true,                                  // setInstructionStateBaseAddress
        true,                                  // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_NE(generalStateBaseAddress, sbaCmd.getGeneralStateBaseAddress());
    EXPECT_EQ(gmmHelper->decanonize(generalStateBaseAddress), sbaCmd.getGeneralStateBaseAddress());
}

HWTEST_F(SbaTest, givenNonZeroGeneralStateBaseAddressWhenProgrammingIsDisabledThenExpectCommandValueZero) {
    constexpr uint64_t generalStateBaseAddress = 0x80010000ull;

    typename FamilyType::STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = {
        generalStateBaseAddress,               // generalStateBase
        0,                                     // indirectObjectHeapBaseAddress
        generalStateBaseAddress,               // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        &sbaCmd,                               // stateBaseAddressCmd
        &dsh,                                  // dsh
        &ioh,                                  // ioh
        &ssh,                                  // ssh
        pDevice->getGmmHelper(),               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        true,                                  // setInstructionStateBaseAddress
        false,                                 // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };
    StateBaseAddressHelper<FamilyType>::programStateBaseAddress(args);

    EXPECT_EQ(0ull, sbaCmd.getGeneralStateBaseAddress());
    EXPECT_EQ(0u, sbaCmd.getGeneralStateBufferSize());
    EXPECT_FALSE(sbaCmd.getGeneralStateBaseAddressModifyEnable());
    EXPECT_FALSE(sbaCmd.getGeneralStateBufferSizeModifyEnable());
}

HWTEST_F(SbaTest, givenNonZeroInternalHeapBaseAddressWhenProgrammingIsDisabledThenExpectCommandValueZero) {
    constexpr uint64_t internalHeapBaseAddress = 0x80010000ull;

    typename FamilyType::STATE_BASE_ADDRESS sbaCmd;
    StateBaseAddressHelperArgs<FamilyType> args = {
        internalHeapBaseAddress,               // generalStateBase
        internalHeapBaseAddress,               // indirectObjectHeapBaseAddress
        0,                                     // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        &sbaCmd,                               // stateBaseAddressCmd
        &dsh,                                  // dsh
        &ioh,                                  // ioh
        &ssh,                                  // ssh
        pDevice->getGmmHelper(),               // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        false,                                 // setInstructionStateBaseAddress
        true,                                  // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };
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

    StateBaseAddressHelperArgs<FamilyType> args = {
        generalStateBase,                      // generalStateBase
        internalHeapBase,                      // indirectObjectHeapBaseAddress
        instructionHeapBase,                   // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        &sbaCmd,                               // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        nullptr,                               // ssh
        gmmHelper,                             // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        true,                                  // setInstructionStateBaseAddress
        true,                                  // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };
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
    StateBaseAddressHelperArgs<FamilyType> args = {
        generalStateBase,                      // generalStateBase
        internalHeapBase,                      // indirectObjectHeapBaseAddress
        instructionHeapBase,                   // instructionHeapBaseAddress
        0,                                     // globalHeapsBaseAddress
        0,                                     // surfaceStateBaseAddress
        &sbaCmd,                               // stateBaseAddressCmd
        nullptr,                               // dsh
        nullptr,                               // ioh
        nullptr,                               // ssh
        gmmHelper,                             // gmmHelper
        nullptr,                               // hwInfo
        0,                                     // statelessMocsIndex
        MemoryCompressionState::NotApplicable, // memoryCompressionState
        true,                                  // setInstructionStateBaseAddress
        true,                                  // setGeneralStateBaseAddress
        false,                                 // useGlobalHeapsBaseAddress
        false,                                 // isMultiOsContextCapable
        false,                                 // useGlobalAtomics
        false,                                 // areMultipleSubDevicesInContext
        false,                                 // overrideSurfaceStateBaseAddress
        false                                  // isDebuggerActive
    };
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

    auto surfaceStateCount = StateBaseAddressHelper<FamilyType>::getMaxBindlessSurfaceStates();
    EXPECT_EQ(surfaceStateCount, sbaCmd.getBindlessSurfaceStateSize());
}
