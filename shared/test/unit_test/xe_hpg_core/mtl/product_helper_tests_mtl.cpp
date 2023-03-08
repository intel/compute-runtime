/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "aubstream/product_family.h"
#include "platforms.h"

using namespace NEO;

using MtlProductHelper = ProductHelperTest;

MTLTEST_F(MtlProductHelper, givenMtlConfigWhenSetupHardwareInfoBaseThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    MtlHwConfig::setupHardwareInfoBase(&hwInfo, false, *compilerProductHelper);

    EXPECT_EQ(336u, gtSystemInfo.TotalVsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalHsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalDsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalGsThreads);
    EXPECT_EQ(64u, gtSystemInfo.TotalPsThreadsWindowerRange);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}

MTLTEST_F(MtlProductHelper, givenMtlConfigWhenSetupHardwareInfoThenGtSystemInfoHasNonZeroValues) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    MtlHwConfig::setupHardwareInfo(&hwInfo, false, *compilerProductHelper);

    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.MaxEuPerSubSlice, 0u);
    EXPECT_GT(gtSystemInfo.MaxSlicesSupported, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_GT(gtSystemInfo.L3BankCount, 0u);

    EXPECT_TRUE(gtSystemInfo.CCSInfo.IsValid);
    EXPECT_GT(gtSystemInfo.CCSInfo.NumberOfCCSEnabled, 0u);
}

MTLTEST_F(MtlProductHelper, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Mtl, productHelper->getAubStreamProductFamily());
}

MTLTEST_F(MtlProductHelper, givenMtlProductHelperWhenIsInitBuiltinAsyncSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isInitBuiltinAsyncSupported(*defaultHwInfo));
}

MTLTEST_F(MtlProductHelper, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

MTLTEST_F(MtlProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {

    EXPECT_FALSE(productHelper->getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(productHelper->getScmPropertyCoherencyRequiredSupport());
    EXPECT_TRUE(productHelper->getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_TRUE(productHelper->getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_TRUE(productHelper->getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(productHelper->getScmPropertyDevicePreemptionModeSupport());

    EXPECT_FALSE(productHelper->getStateBaseAddressPropertyGlobalAtomicsSupport());
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

    EXPECT_FALSE(productHelper->getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_TRUE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}

MTLTEST_F(MtlProductHelper, givenProductHelperWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {

    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(hwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, hwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);

    hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(hwInfo));

    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, hwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

MTLTEST_F(MtlProductHelper, givenCompressionFtrEnabledWhenAskingForPageTableManagerThenReturnCorrectValue) {
    auto hwInfo = *defaultHwInfo;

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    hwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_FALSE(productHelper->isPageTableManagerSupported(hwInfo));

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    hwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_TRUE(productHelper->isPageTableManagerSupported(hwInfo));

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    hwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(productHelper->isPageTableManagerSupported(hwInfo));

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    hwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(productHelper->isPageTableManagerSupported(hwInfo));
}

MTLTEST_F(MtlProductHelper, givenHwIpVersionWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenAllowOnlyOn12700And12710) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto isRcs = false;

    hwInfo.ipVersion.architecture = 12;
    hwInfo.ipVersion.release = 70;
    hwInfo.ipVersion.revision = 0;

    {
        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_TRUE(isBasicWARequired);
    }

    hwInfo.ipVersion.revision = 4;

    {
        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_FALSE(isBasicWARequired);
    }

    hwInfo.ipVersion.release = 71;
    hwInfo.ipVersion.revision = 0;

    {
        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_TRUE(isBasicWARequired);
    }

    hwInfo.ipVersion.revision = 4;

    {
        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_FALSE(isBasicWARequired);
    }
}

MTLTEST_F(MtlProductHelper, givenMtlNotLpgWhenIsBFloat16ConversionSupportedIsCalledThenTrueIsReturned) {
    auto hwInfo = *defaultHwInfo.get();
    uint32_t notLpgArchitecture = 10;

    HardwareIpVersion aotConfig = {0};
    aotConfig.architecture = notLpgArchitecture;
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);

    EXPECT_TRUE(productHelper->isBFloat16ConversionSupported(hwInfo));
}

MTLTEST_F(MtlProductHelper, givenMtlLpgWhenIsBFloat16ConversionSupportedIsCalledThenFalseIsReturned) {
    auto hwInfo = *defaultHwInfo.get();
    uint32_t lpgArchitecture = 12;
    uint32_t lpgRelease = 71;

    HardwareIpVersion aotConfig = {0};
    aotConfig.architecture = lpgArchitecture;
    aotConfig.release = lpgRelease;
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);

    EXPECT_FALSE(productHelper->isBFloat16ConversionSupported(hwInfo));
}

MTLTEST_F(MtlProductHelper, givenMtlLpgMdA0WhengetProductMaxPreferredSlmSizeThen96KbValueReturned) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    PREFERRED_SLM_ALLOCATION_SIZE preferredEnumValue = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K;
    auto hwInfo = *defaultHwInfo.get();
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = AOT::XE_LPG_MD_A0;
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);
    preferredEnumValue = static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(productHelper->getProductMaxPreferredSlmSize(hwInfo, preferredEnumValue));
    EXPECT_EQ(preferredEnumValue, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K);
}

MTLTEST_F(MtlProductHelper, givenMtlLpgLgA0WhengetProductMaxPreferredSlmSizeThen96KbValueReturned) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    PREFERRED_SLM_ALLOCATION_SIZE preferredEnumValue = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K;
    auto hwInfo = *defaultHwInfo.get();
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = AOT::XE_LPG_LG_A0;
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);
    preferredEnumValue = static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(productHelper->getProductMaxPreferredSlmSize(hwInfo, preferredEnumValue));
    EXPECT_EQ(preferredEnumValue, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K);
}
MTLTEST_F(MtlProductHelper, givenMtlLpgMdB0WhengetProductMaxPreferredSlmSizeThenPassedValueReturned) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    PREFERRED_SLM_ALLOCATION_SIZE preferredEnumValue = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K;
    auto hwInfo = *defaultHwInfo.get();
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = AOT::XE_LPG_MD_B0;
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);
    preferredEnumValue = static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(productHelper->getProductMaxPreferredSlmSize(hwInfo, preferredEnumValue));
    EXPECT_EQ(preferredEnumValue, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K);
}

MTLTEST_F(MtlProductHelper, givenMtlLpgLgB0WhengetProductMaxPreferredSlmSizeThenPassedValueReturned) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    PREFERRED_SLM_ALLOCATION_SIZE preferredEnumValue = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K;
    auto hwInfo = *defaultHwInfo.get();
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = AOT::XE_LPG_LG_B0;
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);
    preferredEnumValue = static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(productHelper->getProductMaxPreferredSlmSize(hwInfo, preferredEnumValue));
    EXPECT_EQ(preferredEnumValue, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K);
}
