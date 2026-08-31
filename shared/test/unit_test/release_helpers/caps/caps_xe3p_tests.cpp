/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/caps/caps_xe3p.h"

#include "gtest/gtest.h"

#include <cstdint>
#include <optional>

using namespace NEO;

namespace {
HardwareIpVersion withUnsupportedRevision(uint32_t ipVersionValue) {
    HardwareIpVersion ipVersion{ipVersionValue};
    ipVersion.revision = 0x3f;
    return ipVersion;
}
} // namespace

TEST(CapsXe3pTest, givenCriIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsCri>(), resolveCapsCri(AOT::CRI_A0));
    EXPECT_EQ(std::nullopt, resolveCapsCri(withUnsupportedRevision(AOT::CRI_A0)));
}

TEST(CapsXe3pTest, givenNvlPIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsNvlPA0>(), resolveCapsNvlP(AOT::NVL_P_A0));
    EXPECT_EQ(materializeCaps<CapsNvlPB0>(), resolveCapsNvlP(AOT::NVL_P_B0));
    EXPECT_EQ(std::nullopt, resolveCapsNvlP(withUnsupportedRevision(AOT::NVL_P_A0)));
}

TEST(CapsXe3pTest, givenCriReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsCri = materializeCaps<CapsCri>();
    EXPECT_FALSE(capsCri.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsCri.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsCri.availableSemaphore64);
    EXPECT_TRUE(capsCri.bFloat16ConversionSupported);
    EXPECT_TRUE(capsCri.bindlessAddressingDisabled);
    EXPECT_TRUE(capsCri.blitImageAllowedForDepthFormat);
    EXPECT_TRUE(capsCri.deviceConfigStringTileCountIncluded);
    EXPECT_TRUE(capsCri.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsCri.directSubmissionLightSupported);
    EXPECT_TRUE(capsCri.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsCri.dummyBlitWaRequired);
    EXPECT_FALSE(capsCri.forceEmuInt32DivRemSPRequired);
    EXPECT_FALSE(capsCri.ftrXe2Compression);
    EXPECT_TRUE(capsCri.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsCri.latePreemptionStartSupported);
    EXPECT_FALSE(capsCri.localOnlyAllowed);
    EXPECT_TRUE(capsCri.matrixMultiplyAccumulateSupported);
    EXPECT_FALSE(capsCri.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsCri.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsCri.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_TRUE(capsCri.postImageWriteFlushRequired);
    EXPECT_FALSE(capsCri.preImageReadFlushRequired);
    EXPECT_FALSE(capsCri.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsCri.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsCri.queryPeerAccess);
    EXPECT_FALSE(capsCri.rayTracingSupported);
    EXPECT_TRUE(capsCri.rcsExposureDisabled);
    EXPECT_FALSE(capsCri.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsCri.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsCri.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3pTest, givenNvlPReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsNvlPA0 = materializeCaps<CapsNvlPA0>();
    constexpr auto capsNvlPB0 = materializeCaps<CapsNvlPB0>();

    EXPECT_FALSE(capsNvlPA0.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsNvlPA0.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsNvlPA0.availableSemaphore64);
    EXPECT_TRUE(capsNvlPA0.bFloat16ConversionSupported);
    EXPECT_TRUE(capsNvlPA0.bindlessAddressingDisabled);
    EXPECT_TRUE(capsNvlPA0.blitImageAllowedForDepthFormat);
    EXPECT_TRUE(capsNvlPA0.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsNvlPA0.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsNvlPA0.directSubmissionLightSupported);
    EXPECT_TRUE(capsNvlPA0.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsNvlPA0.dummyBlitWaRequired);
    EXPECT_FALSE(capsNvlPA0.forceEmuInt32DivRemSPRequired);
    EXPECT_TRUE(capsNvlPA0.ftrXe2Compression);
    EXPECT_TRUE(capsNvlPA0.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsNvlPA0.latePreemptionStartSupported);
    EXPECT_FALSE(capsNvlPA0.localOnlyAllowed);
    EXPECT_TRUE(capsNvlPA0.matrixMultiplyAccumulateSupported);
    EXPECT_FALSE(capsNvlPA0.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsNvlPA0.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsNvlPA0.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_TRUE(capsNvlPA0.postImageWriteFlushRequired);
    EXPECT_TRUE(capsNvlPA0.preImageReadFlushRequired);
    EXPECT_FALSE(capsNvlPA0.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsNvlPA0.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsNvlPA0.queryPeerAccess);
    EXPECT_TRUE(capsNvlPA0.rayTracingSupported);
    EXPECT_TRUE(capsNvlPA0.rcsExposureDisabled);
    EXPECT_FALSE(capsNvlPA0.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsNvlPA0.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsNvlPA0.splitMatrixMultiplyAccumulateSupported);

    EXPECT_FALSE(capsNvlPB0.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsNvlPB0.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsNvlPB0.availableSemaphore64);
    EXPECT_TRUE(capsNvlPB0.bFloat16ConversionSupported);
    EXPECT_TRUE(capsNvlPB0.bindlessAddressingDisabled);
    EXPECT_TRUE(capsNvlPB0.blitImageAllowedForDepthFormat);
    EXPECT_TRUE(capsNvlPB0.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsNvlPB0.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsNvlPB0.directSubmissionLightSupported);
    EXPECT_TRUE(capsNvlPB0.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsNvlPB0.dummyBlitWaRequired);
    EXPECT_FALSE(capsNvlPB0.forceEmuInt32DivRemSPRequired);
    EXPECT_TRUE(capsNvlPB0.ftrXe2Compression);
    EXPECT_TRUE(capsNvlPB0.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsNvlPB0.latePreemptionStartSupported);
    EXPECT_FALSE(capsNvlPB0.localOnlyAllowed);
    EXPECT_TRUE(capsNvlPB0.matrixMultiplyAccumulateSupported);
    EXPECT_FALSE(capsNvlPB0.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsNvlPB0.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsNvlPB0.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_TRUE(capsNvlPB0.postImageWriteFlushRequired);
    EXPECT_TRUE(capsNvlPB0.preImageReadFlushRequired);
    EXPECT_FALSE(capsNvlPB0.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsNvlPB0.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsNvlPB0.queryPeerAccess);
    EXPECT_TRUE(capsNvlPB0.rayTracingSupported);
    EXPECT_TRUE(capsNvlPB0.rcsExposureDisabled);
    EXPECT_FALSE(capsNvlPB0.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsNvlPB0.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsNvlPB0.splitMatrixMultiplyAccumulateSupported);
}
