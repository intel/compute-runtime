/*
 * Copyright (C) 2021-2023 Intel Corporation
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

#include "platforms.h"

using namespace NEO;

using CommandEncodeDG2Test = ::testing::Test;

DG2TEST_F(CommandEncodeDG2Test, whenProgrammingStateComputeModeThenProperFieldsAreSet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIXEL_ASYNC_COMPUTE_THREAD_LIMIT = typename STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT;
    using Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT = typename STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT;
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

DG2TEST_F(CommandEncodeDG2Test, whenProgramComputeWalkerThenApplyL3WAForDg2G10A0) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;
    auto walkerCmd = FamilyType::cmdInitGpgpuWalker;
    MockExecutionEnvironment executionEnvironment{};
    auto &compilerProductHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();

    std::vector<std::pair<unsigned short, uint16_t>> dg2Configs =
        {{dg2G10DeviceIds[0], REV_ID_A0},
         {dg2G10DeviceIds[0], REV_ID_A1},
         {dg2G10DeviceIds[0], REV_ID_B0},
         {dg2G10DeviceIds[0], REV_ID_C0},
         {dg2G11DeviceIds[0], REV_ID_A0},
         {dg2G11DeviceIds[0], REV_ID_B0},
         {dg2G11DeviceIds[0], REV_ID_B1},
         {dg2G12DeviceIds[0], REV_ID_A0}};

    KernelDescriptor kernelDescriptor;
    EncodeWalkerArgs walkerArgs{KernelExecutionType::Default, true, kernelDescriptor};

    for (const auto &[deviceID, revisionID] : dg2Configs) {
        hwInfo.platform.usRevId = revisionID;
        hwInfo.platform.usDeviceID = deviceID;
        hwInfo.ipVersion = compilerProductHelper.getHwIpVersion(hwInfo);
        rootDeviceEnvironment.releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);

        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);

        if (DG2::isG10(hwInfo) && revisionID < REV_ID_B0) {
            EXPECT_TRUE(walkerCmd.getL3PrefetchDisable());
        } else {
            EXPECT_FALSE(walkerCmd.getL3PrefetchDisable());
        }
    }
}

using Dg2SbaTest = SbaTest;

DG2TEST_F(Dg2SbaTest, givenSpecificProductFamilyWhenAppendingSbaThenProgramCorrectL1CachePolicy) {
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
    args.setGeneralStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, sbaCmd.getL1CachePolicyL1CacheControl());

    args.isDebuggerActive = true;
    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, sbaCmd.getL1CachePolicyL1CacheControl());
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

    EXPECT_EQ(0u, sbaCmd.getL1CachePolicyL1CacheControl());

    debugManager.flags.ForceStatelessL1CachingPolicy.set(2u);
    updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(2u, sbaCmd.getL1CachePolicyL1CacheControl());

    debugManager.flags.ForceAllResourcesUncached.set(true);
    updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(1u, sbaCmd.getL1CachePolicyL1CacheControl());
}
