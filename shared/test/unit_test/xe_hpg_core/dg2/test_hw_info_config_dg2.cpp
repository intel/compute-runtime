/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

using namespace NEO;

using Dg2ProductHelper = ProductHelperTest;

DG2TEST_F(Dg2ProductHelper, givenDG2WithCSteppingThenAdditionalStateBaseAddressWAIsNotRequired) {
    pInHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_C, pInHwInfo);
    auto isWARequired = productHelper->isAdditionalStateBaseAddressWARequired(pInHwInfo);
    EXPECT_FALSE(isWARequired);
}

DG2TEST_F(Dg2ProductHelper, givenDG2WithBSteppingThenAdditionalStateBaseAddressWAIsRequired) {
    pInHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, pInHwInfo);
    auto isWARequired = productHelper->isAdditionalStateBaseAddressWARequired(pInHwInfo);
    EXPECT_TRUE(isWARequired);
}

DG2TEST_F(Dg2ProductHelper, givenDG2WithA0SteppingThenMaxThreadsForWorkgroupWAIsRequired) {
    pInHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A0, pInHwInfo);
    for (const auto &devId : dg2G10DeviceIds) {
        pInHwInfo.platform.usDeviceID = devId;
        auto isWARequired = productHelper->isMaxThreadsForWorkgroupWARequired(pInHwInfo);
        EXPECT_TRUE(isWARequired);
    }
}

DG2TEST_F(Dg2ProductHelper, givenDG2WithBSteppingThenMaxThreadsForWorkgroupWAIsNotRequired) {
    pInHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A1, pInHwInfo);
    auto isWARequired = productHelper->isMaxThreadsForWorkgroupWARequired(pInHwInfo);
    EXPECT_FALSE(isWARequired);
}

DG2TEST_F(Dg2ProductHelper, givenSteppingWhenAskingForLocalMemoryAccessModeThenDisallowOnA0) {
    pInHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_A0, pInHwInfo);
    EXPECT_EQ(LocalMemoryAccessMode::CpuAccessDisallowed, productHelper->getLocalMemoryAccessMode(pInHwInfo));

    pInHwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, pInHwInfo);
    EXPECT_EQ(LocalMemoryAccessMode::Default, productHelper->getLocalMemoryAccessMode(pInHwInfo));
}

DG2TEST_F(Dg2ProductHelper, givenDG2ProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(pInHwInfo));
}

DG2TEST_F(Dg2ProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {

    EXPECT_FALSE(productHelper->getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(productHelper->getScmPropertyCoherencyRequiredSupport());
    EXPECT_TRUE(productHelper->getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_TRUE(productHelper->getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_TRUE(productHelper->getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(productHelper->getScmPropertyDevicePreemptionModeSupport());

    EXPECT_FALSE(productHelper->getStateBaseAddressPropertyGlobalAtomicsSupport());
    EXPECT_TRUE(productHelper->getStateBaseAddressPropertyStatelessMocsSupport());
    EXPECT_TRUE(productHelper->getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport());

    EXPECT_TRUE(productHelper->getFrontEndPropertyScratchSizeSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyStateSipSupport());
    EXPECT_FALSE(productHelper->getPreemptionDbgPropertyCsrSurfaceSupport());

    EXPECT_FALSE(productHelper->getFrontEndPropertyComputeDispatchAllWalkerSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_TRUE(productHelper->getPipelineSelectPropertyModeSelectedSupport());
    EXPECT_FALSE(productHelper->getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_TRUE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}
