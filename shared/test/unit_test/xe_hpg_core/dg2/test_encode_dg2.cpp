/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/helpers/state_base_address_tests.h"

#include "neo_aot_platforms.h"

using namespace NEO;

using CommandEncodeDG2Test = ::testing::Test;

DG2TEST_F(CommandEncodeDG2Test, whenProgrammingStateComputeModeThenProperFieldsAreSet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    uint8_t buffer[64]{};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    StateComputeModeProperties properties;
    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
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
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    expectedMask |= FamilyType::stateComputeModeZPassAsyncComputeThreadLimitMask |
                    FamilyType::stateComputeModePixelAsyncComputeThreadLimitMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED, pScm->getForceNonCoherent());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_2, pScm->getPixelAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64, pScm->getZPassAsyncComputeThreadLimit());
    EXPECT_TRUE(pScm->getLargeGrfMode());
}

DG2TEST_F(CommandEncodeDG2Test, whenProgramComputeWalkerThenSetL3PrefetchDefaultValue) {
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;
    auto idd = FamilyType::cmdInitInterfaceDescriptorData;

    EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);
    EXPECT_FALSE(walkerCmd.getL3PrefetchDisable());
}

using Dg2SbaTest = SbaTest;

DG2TEST_F(Dg2SbaTest, givenSpecificProductFamilyWhenAppendingSbaThenProgramCorrectL1CachePolicy) {
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
    args.setGeneralStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WB, sbaCmd.getL1CacheControlCachePolicy());

    args.isDebuggerActive = true;
    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, sbaCmd.getL1CacheControlCachePolicy());
}

DG2TEST_F(Dg2SbaTest, givenL1CachingOverrideWhenStateBaseAddressIsProgrammedThenItMatchesTheOverrideValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceStatelessL1CachingPolicy.set(0u);
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
    args.setGeneralStateBaseAddress = true;
    auto &productHelper = getHelper<ProductHelper>();
    updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(0u, sbaCmd.getL1CacheControlCachePolicy());

    debugManager.flags.ForceStatelessL1CachingPolicy.set(2u);
    updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(2u, sbaCmd.getL1CacheControlCachePolicy());

    debugManager.flags.ForceAllResourcesUncached.set(true);
    updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(1u, sbaCmd.getL1CacheControlCachePolicy());
}
