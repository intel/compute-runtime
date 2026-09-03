/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/release_helpers/caps/caps_setup.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(CapsSetupTest, givenEveryEnabledProductConfigWhenResolvingCapsThenCapsAreReturned) {
    ProductConfigHelper productConfigHelper{};
    const auto &deviceAotInfos = productConfigHelper.getDeviceAotInfo();
    ASSERT_FALSE(deviceAotInfos.empty());

    for (const auto &deviceAotInfo : deviceAotInfos) {
        EXPECT_TRUE(resolveCaps(deviceAotInfo.aotConfig).has_value());
    }
}

TEST(CapsSetupTest, givenEveryEnabledProductConfigWhenSettingUpCapsThenHwInfoIsInitializedWithResolvedCaps) {
    ProductConfigHelper productConfigHelper{};

    for (const auto &deviceAotInfo : productConfigHelper.getDeviceAotInfo()) {
        ASSERT_NE(nullptr, deviceAotInfo.hwInfo);

        HardwareInfo hwInfo = *deviceAotInfo.hwInfo;
        hwInfo.caps = {};
        hwInfo.ipVersion = deviceAotInfo.aotConfig;

        setupCaps(hwInfo);

        auto expectedCaps = resolveCaps(deviceAotInfo.aotConfig);
        ASSERT_TRUE(expectedCaps.has_value());
        EXPECT_EQ(expectedCaps->kernelBFloat16AtomicCapabilities, hwInfo.caps.kernelBFloat16AtomicCapabilities);
        EXPECT_EQ(expectedCaps->kernelFp16AtomicCapabilities, hwInfo.caps.kernelFp16AtomicCapabilities);
        EXPECT_EQ(expectedCaps->adjustWalkOrderAvailable, hwInfo.caps.adjustWalkOrderAvailable);
        EXPECT_EQ(expectedCaps->auxSurfaceModeOverrideRequired, hwInfo.caps.auxSurfaceModeOverrideRequired);
        EXPECT_EQ(expectedCaps->availableSemaphore64, hwInfo.caps.availableSemaphore64);
        EXPECT_EQ(expectedCaps->bFloat16ConversionSupported, hwInfo.caps.bFloat16ConversionSupported);
        EXPECT_EQ(expectedCaps->bindlessAddressingDisabled, hwInfo.caps.bindlessAddressingDisabled);
        EXPECT_EQ(expectedCaps->blitImageAllowedForDepthFormat, hwInfo.caps.blitImageAllowedForDepthFormat);
        EXPECT_EQ(expectedCaps->deviceConfigStringTileCountIncluded, hwInfo.caps.deviceConfigStringTileCountIncluded);
        EXPECT_EQ(expectedCaps->deviceConfigStringXeCuSegmentIncluded, hwInfo.caps.deviceConfigStringXeCuSegmentIncluded);
        EXPECT_EQ(expectedCaps->directSubmissionLightSupported, hwInfo.caps.directSubmissionLightSupported);
        EXPECT_EQ(expectedCaps->dotProductAccumulateSystolicSupported, hwInfo.caps.dotProductAccumulateSystolicSupported);
        EXPECT_EQ(expectedCaps->dummyBlitWaRequired, hwInfo.caps.dummyBlitWaRequired);
        EXPECT_EQ(expectedCaps->forceEmuInt32DivRemSPRequired, hwInfo.caps.forceEmuInt32DivRemSPRequired);
        EXPECT_EQ(expectedCaps->ftrXe2Compression, hwInfo.caps.ftrXe2Compression);
        EXPECT_EQ(expectedCaps->globalBindlessAllocatorEnabled, hwInfo.caps.globalBindlessAllocatorEnabled);
        EXPECT_EQ(expectedCaps->latePreemptionStartSupported, hwInfo.caps.latePreemptionStartSupported);
        EXPECT_EQ(expectedCaps->localOnlyAllowed, hwInfo.caps.localOnlyAllowed);
        EXPECT_EQ(expectedCaps->matrixMultiplyAccumulateSupported, hwInfo.caps.matrixMultiplyAccumulateSupported);
        EXPECT_EQ(expectedCaps->numRtStacksPerDssFixedValue, hwInfo.caps.numRtStacksPerDssFixedValue);
        EXPECT_EQ(expectedCaps->pipeControlPriorToNonPipelinedStateCommandsBaseWARequired, hwInfo.caps.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
        EXPECT_EQ(expectedCaps->pipeControlPriorToPipelineSelectWaRequired, hwInfo.caps.pipeControlPriorToPipelineSelectWaRequired);
        EXPECT_EQ(expectedCaps->postImageWriteFlushRequired, hwInfo.caps.postImageWriteFlushRequired);
        EXPECT_EQ(expectedCaps->preImageReadFlushRequired, hwInfo.caps.preImageReadFlushRequired);
        EXPECT_EQ(expectedCaps->programAdditionalStallPriorToBarrierWithTimestamp, hwInfo.caps.programAdditionalStallPriorToBarrierWithTimestamp);
        EXPECT_EQ(expectedCaps->programAllStateComputeCommandFieldsWARequired, hwInfo.caps.programAllStateComputeCommandFieldsWARequired);
        EXPECT_EQ(expectedCaps->queryPeerAccess, hwInfo.caps.queryPeerAccess);
        EXPECT_EQ(expectedCaps->rayTracingSupported, hwInfo.caps.rayTracingSupported);
        EXPECT_EQ(expectedCaps->rcsExposureDisabled, hwInfo.caps.rcsExposureDisabled);
        EXPECT_EQ(expectedCaps->reducedSurfaceStateSupported, hwInfo.caps.reducedSurfaceStateSupported);
        EXPECT_EQ(expectedCaps->singleDispatchRequiredForMultiCCS, hwInfo.caps.singleDispatchRequiredForMultiCCS);
        EXPECT_EQ(expectedCaps->splitMatrixMultiplyAccumulateSupported, hwInfo.caps.splitMatrixMultiplyAccumulateSupported);
    }
}

TEST(CapsSetupTest, givenUnknownIpVersionWhenResolvingCapsThenNulloptIsReturned) {
    HardwareIpVersion unknownIpVersion{};
    unknownIpVersion.architecture = 0x3ff;
    unknownIpVersion.release = 0xff;
    unknownIpVersion.revision = 0x3f;

    EXPECT_FALSE(resolveCaps(unknownIpVersion).has_value());
}

TEST(CapsTest, givenDefaultCapsThenValuesAreCorrect) {
    constexpr Caps caps{};

    EXPECT_EQ(0u, caps.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(0u, caps.kernelFp16AtomicCapabilities);
    EXPECT_FALSE(caps.adjustWalkOrderAvailable);
    EXPECT_FALSE(caps.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(caps.availableSemaphore64);
    EXPECT_FALSE(caps.bFloat16ConversionSupported);
    EXPECT_FALSE(caps.bindlessAddressingDisabled);
    EXPECT_FALSE(caps.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(caps.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(caps.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(caps.directSubmissionLightSupported);
    EXPECT_FALSE(caps.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(caps.dummyBlitWaRequired);
    EXPECT_FALSE(caps.forceEmuInt32DivRemSPRequired);
    EXPECT_FALSE(caps.ftrXe2Compression);
    EXPECT_FALSE(caps.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(caps.latePreemptionStartSupported);
    EXPECT_FALSE(caps.localOnlyAllowed);
    EXPECT_FALSE(caps.matrixMultiplyAccumulateSupported);
    EXPECT_FALSE(caps.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(caps.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(caps.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(caps.postImageWriteFlushRequired);
    EXPECT_FALSE(caps.preImageReadFlushRequired);
    EXPECT_FALSE(caps.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(caps.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(caps.queryPeerAccess);
    EXPECT_FALSE(caps.rayTracingSupported);
    EXPECT_FALSE(caps.rcsExposureDisabled);
    EXPECT_FALSE(caps.reducedSurfaceStateSupported);
    EXPECT_FALSE(caps.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(caps.splitMatrixMultiplyAccumulateSupported);
}
