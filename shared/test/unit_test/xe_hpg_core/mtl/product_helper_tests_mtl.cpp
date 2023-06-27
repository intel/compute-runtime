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
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
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

MTLTEST_F(MtlProductHelper, givenMtlProductHelperWhenCheckDirectSubmissionConstantCacheInvalidationNeededThenTrueIsReturned) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(productHelper->isDirectSubmissionConstantCacheInvalidationNeeded(hwInfo));
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
    refreshReleaseHelper(&hwInfo);

    {
        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_TRUE(isBasicWARequired);
    }

    hwInfo.ipVersion.revision = 4;
    refreshReleaseHelper(&hwInfo);

    {
        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_FALSE(isBasicWARequired);
    }

    hwInfo.ipVersion.release = 71;
    hwInfo.ipVersion.revision = 0;
    refreshReleaseHelper(&hwInfo);

    {
        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_TRUE(isBasicWARequired);
    }

    hwInfo.ipVersion.revision = 4;
    refreshReleaseHelper(&hwInfo);

    {
        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_FALSE(isBasicWARequired);
    }
}

MTLTEST_F(MtlProductHelper, givenNoReleaseHelperWhenisPipeControlPriorToNonPipelinedStateCommandsWARequiredThenCorrectValuesAreReturned) {
    auto &rootDeviceEnvironment = *this->executionEnvironment->rootDeviceEnvironments[0].get();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    ReleaseHelper *releaseHelper = nullptr;
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, false, releaseHelper);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_TRUE(isBasicWARequired);
}

MTLTEST_F(MtlProductHelper, givenMtlNotLpgWhenIsBFloat16ConversionSupportedIsCalledThenTrueIsReturned) {
    auto hwInfo = *defaultHwInfo.get();
    uint32_t notLpgArchitecture = 10;

    HardwareIpVersion aotConfig = {0};
    aotConfig.architecture = notLpgArchitecture;
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);

    EXPECT_TRUE(compilerProductHelper.isBFloat16ConversionSupported(hwInfo));
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

    EXPECT_FALSE(compilerProductHelper.isBFloat16ConversionSupported(hwInfo));
}

MTLTEST_F(MtlProductHelper, givenMtlMA0WhenGetProductMaxPreferredSlmSizeThen96KbValueReturned) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    PREFERRED_SLM_ALLOCATION_SIZE preferredEnumValue = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K;
    auto hwInfo = *defaultHwInfo.get();
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = AOT::MTL_M_A0;
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);
    refreshReleaseHelper(&hwInfo);
    preferredEnumValue = static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(releaseHelper->getProductMaxPreferredSlmSize(preferredEnumValue));
    EXPECT_EQ(preferredEnumValue, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K);
}

MTLTEST_F(MtlProductHelper, givenMtlPA0WhenGetProductMaxPreferredSlmSizeThen96KbValueReturned) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    PREFERRED_SLM_ALLOCATION_SIZE preferredEnumValue = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K;
    auto hwInfo = *defaultHwInfo.get();
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = AOT::MTL_P_A0;
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);
    refreshReleaseHelper(&hwInfo);
    preferredEnumValue = static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(releaseHelper->getProductMaxPreferredSlmSize(preferredEnumValue));
    EXPECT_EQ(preferredEnumValue, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K);
}
MTLTEST_F(MtlProductHelper, givenMtlMB0WhenGetProductMaxPreferredSlmSizeThenPassedValueReturned) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    PREFERRED_SLM_ALLOCATION_SIZE preferredEnumValue = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K;
    auto hwInfo = *defaultHwInfo.get();
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = AOT::MTL_M_B0;
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);
    refreshReleaseHelper(&hwInfo);
    preferredEnumValue = static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(releaseHelper->getProductMaxPreferredSlmSize(preferredEnumValue));
    EXPECT_EQ(preferredEnumValue, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K);
}

MTLTEST_F(MtlProductHelper, givenMtlPB0WhenGetProductMaxPreferredSlmSizeThenPassedValueReturned) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename XeHpgCoreFamily::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    PREFERRED_SLM_ALLOCATION_SIZE preferredEnumValue = PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K;
    auto hwInfo = *defaultHwInfo.get();
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = AOT::MTL_P_B0;
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);
    refreshReleaseHelper(&hwInfo);
    preferredEnumValue = static_cast<PREFERRED_SLM_ALLOCATION_SIZE>(releaseHelper->getProductMaxPreferredSlmSize(preferredEnumValue));
    EXPECT_EQ(preferredEnumValue, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K);
}

MTLTEST_F(MtlProductHelper, givenMtlWhenCallIsAdjustWalkOrderAvailableThenReturnProperValue) {
    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    unsigned int gmdReleases[] = {70, 71};
    defaultHwInfo->ipVersion.architecture = 12;

    for (auto gmdRelease : gmdReleases) {
        defaultHwInfo->ipVersion.release = gmdRelease;
        refreshReleaseHelper(defaultHwInfo.get());
        EXPECT_EQ(!MTL::isLpg(*defaultHwInfo), productHelper->isAdjustWalkOrderAvailable(releaseHelper));
    }
}

MTLTEST_F(MtlProductHelper, givenMtlAndReleaseHelperNullptrWhengetIsMatrixMultiplyAccumulateSupportedThenReturnsFalse) {
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    ReleaseHelper *releaseHelper = nullptr;
    EXPECT_FALSE(compilerProductHelper.isMatrixMultiplyAccumulateSupported(releaseHelper));
}

MTLTEST_F(MtlProductHelper, givenVariousMtlReleasesWhenGetExtensionsIsCalledThenMatrixMultiplyAccumulateExtensionsAreCorrectlyReported) {

    auto &rootDeviceEnvironment = *this->executionEnvironment->rootDeviceEnvironments[0].get();
    auto &compilerProductHelper = this->executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    unsigned int gmdReleases[] = {70, 71};
    hwInfo.ipVersion.architecture = 12;

    for (auto gmdRelease : gmdReleases) {
        hwInfo.ipVersion.release = gmdRelease;

        refreshReleaseHelper(&hwInfo);
        auto releaseHelper = rootDeviceEnvironment.getReleaseHelper();
        auto extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);

        EXPECT_EQ(!MTL::isLpg(hwInfo), hasSubstr(extensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
        EXPECT_EQ(!MTL::isLpg(hwInfo), hasSubstr(extensions, std::string("cl_intel_subgroup_split_matrix_multiply_accumulate")));
        EXPECT_EQ(!MTL::isLpg(hwInfo), compilerProductHelper.isMatrixMultiplyAccumulateSupported(releaseHelper));
        EXPECT_EQ(!MTL::isLpg(hwInfo), compilerProductHelper.isSplitMatrixMultiplyAccumulateSupported(hwInfo));
    }
}

MTLTEST_F(MtlProductHelper, givenCompilerProductHelperWhenGetDefaultHwIpVersionThenCorrectValueIsSet) {
    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), AOT::MTL_M_A0);
}

MTLTEST_F(MtlProductHelper, givenDebugFlagWhenCheckingIsResolveDependenciesByPipeControlsSupportedThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiver csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr.taskCount = 2;

    // ResolveDependenciesViaPipeControls = -1 (default)
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));

    DebugManager.flags.ResolveDependenciesViaPipeControls.set(0);
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));

    DebugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));
}

using ProductHelperTestMtl = Test<DeviceFixture>;

MTLTEST_F(ProductHelperTestMtl, givenPatIndexAndAllocationTypeWhenCallOverridePatIndexThenForTimestampPacketTagBufferReturnTwo) {
    auto &helper = getHelper<ProductHelper>();
    uint64_t expectedPatIndexWhenTimestampPacketTagBuffer = 2u;
    uint64_t patIndex = 1u;
    auto allocationType = NEO::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER;
    EXPECT_EQ(expectedPatIndexWhenTimestampPacketTagBuffer, helper.overridePatIndex(allocationType, patIndex));
    allocationType = NEO::AllocationType::BUFFER;
    patIndex = 3u;
    EXPECT_EQ(patIndex, helper.overridePatIndex(allocationType, patIndex));
}

MTLTEST_F(ProductHelperTestMtl, givenMtlWhenCheckIsCachingOnCpuAvailableThenAlwaysFalse) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(productHelper.isCachingOnCpuAvailable());
}
