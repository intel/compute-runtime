/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "aubstream/product_family.h"
#include "platforms.h"

using namespace NEO;

struct XeLpgProductHelperTests : public ::Test<DeviceFixture> {
    void SetUp() override {
        ::Test<DeviceFixture>::SetUp();
        productHelper = &pDevice->getProductHelper();
    }
    ProductHelper const *productHelper = nullptr;
};

HWTEST2_F(XeLpgProductHelperTests, whenCheckPreferredAllocationMethodThenAllocateByKmdIsReturned, IsXeLpg) {
    for (auto i = 0; i < static_cast<int>(AllocationType::COUNT); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        auto preferredAllocationMethod = productHelper->getPreferredAllocationMethod(allocationType);
        if (allocationType == AllocationType::TAG_BUFFER ||
            allocationType == AllocationType::TIMESTAMP_PACKET_TAG_BUFFER) {
            EXPECT_FALSE(preferredAllocationMethod.has_value());
        } else {
            EXPECT_TRUE(preferredAllocationMethod.has_value());
            EXPECT_EQ(GfxMemoryAllocationMethod::AllocateByKmd, preferredAllocationMethod.value());
        }
    }
}

HWTEST2_F(XeLpgProductHelperTests, givenBooleanUncachedWhenCallOverridePatIndexThenProperPatIndexIsReturned, IsXeLpg) {
    uint64_t patIndex = 1u;
    bool isUncached = true;
    EXPECT_EQ(2u, productHelper->overridePatIndex(isUncached, patIndex));

    isUncached = false;
    EXPECT_EQ(patIndex, productHelper->overridePatIndex(isUncached, patIndex));
}

HWTEST2_F(XeLpgProductHelperTests, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned, IsXeLpg) {
    EXPECT_EQ(aub_stream::ProductFamily::Mtl, productHelper->getAubStreamProductFamily());
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned, IsXeLpg) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(*defaultHwInfo));
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenCheckDirectSubmissionConstantCacheInvalidationNeededThenTrueIsReturned, IsXeLpg) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(productHelper->isDirectSubmissionConstantCacheInvalidationNeeded(hwInfo));
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenIsInitBuiltinAsyncSupportedThenReturnFalse, IsXeLpg) {
    EXPECT_FALSE(productHelper->isInitBuiltinAsyncSupported(*defaultHwInfo));
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue, IsXeLpg) {
    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues, IsXeLpg) {

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

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned, IsXeLpg) {

    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(hwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, hwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

HWTEST2_F(XeLpgProductHelperTests, givenCompressionFtrEnabledWhenAskingForPageTableManagerThenReturnCorrectValue, IsXeLpg) {
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

HWTEST2_F(XeLpgProductHelperTests, givenHwIpVersionWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenAllowBasedOnReleaseHelper, IsXeLpg) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto isRcs = false;

    AOT::PRODUCT_CONFIG ipReleases[] = {AOT::MTL_M_A0, AOT::MTL_M_B0, AOT::MTL_P_A0, AOT::MTL_P_B0};
    for (auto &ipRelease : ipReleases) {
        hwInfo.ipVersion.value = ipRelease;
        auto releaseHelper = ReleaseHelper::create(ipRelease);

        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper.get());

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_EQ(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(), isBasicWARequired);
    }
}

HWTEST2_F(XeLpgProductHelperTests, givenReleaseHelperNullptrWhenCallingGetMediaFrequencyTileIndexThenReturnFalse, IsXeLpg) {
    uint32_t tileIndex = 0;
    ReleaseHelper *releaseHelper = nullptr;
    EXPECT_FALSE(productHelper->getMediaFrequencyTileIndex(releaseHelper, tileIndex));
}

HWTEST2_F(XeLpgProductHelperTests, whenCheckIsCachingOnCpuAvailableThenAlwaysFalse, IsXeLpg) {
    EXPECT_FALSE(productHelper->isCachingOnCpuAvailable());
}

HWTEST2_F(XeLpgProductHelperTests, whenCheckFp64SupportThenReturnTrue, IsXeLpg) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64);
}