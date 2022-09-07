/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/helpers/state_base_address_tests.h"

using namespace NEO;

using CommandEncodeDG2Test = ::testing::Test;

DG2TEST_F(CommandEncodeDG2Test, whenProgrammingStateComputeModeThenProperFieldsAreSet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIXEL_ASYNC_COMPUTE_THREAD_LIMIT = typename STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT;
    using Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT = typename STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT;
    uint8_t buffer[64]{};

    StateComputeModeProperties properties;
    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, *defaultHwInfo, nullptr);
    auto pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    auto expectedMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT, pScm->getForceNonCoherent());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.value = 1;
    properties.zPassAsyncComputeThreadLimit.value = 1;
    properties.pixelAsyncComputeThreadLimit.value = 1;
    properties.largeGrfMode.value = 1;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, *defaultHwInfo, nullptr);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    expectedMask |= FamilyType::stateComputeModeZPassAsyncComputeThreadLimitMask |
                    FamilyType::stateComputeModePixelAsyncComputeThreadLimitMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED, pScm->getForceNonCoherent());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_2, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_TRUE(pScm->getLargeGrfMode());
}

using Dg2SbaTest = SbaTest;

DG2TEST_F(Dg2SbaTest, givenSpecificProductFamilyWhenAppendingSbaThenProgramCorrectL1CachePolicy) {
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
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
    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, sbaCmd.getL1CachePolicyL1CacheControl());

    args.isDebuggerActive = true;
    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, sbaCmd.getL1CachePolicyL1CacheControl());
}

DG2TEST_F(Dg2SbaTest, givenL1CachingOverrideWhenStateBaseAddressIsProgrammedThenItMatchesTheOverrideValue) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ForceStatelessL1CachingPolicy.set(0u);
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
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
        false                                               // overrideSurfaceStateBaseAddress
    };
    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);

    EXPECT_EQ(0u, sbaCmd.getL1CachePolicyL1CacheControl());

    DebugManager.flags.ForceStatelessL1CachingPolicy.set(2u);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);

    EXPECT_EQ(2u, sbaCmd.getL1CachePolicyL1CacheControl());

    DebugManager.flags.ForceAllResourcesUncached.set(true);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args, true);

    EXPECT_EQ(1u, sbaCmd.getL1CachePolicyL1CacheControl());
}
