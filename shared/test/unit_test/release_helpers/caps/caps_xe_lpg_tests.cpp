/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/caps/caps_xe_lpg.h"

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

TEST(CapsXeLpgTest, givenMtlUIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsMtlUA0>(), resolveCapsMtlU(AOT::MTL_U_A0));
    EXPECT_EQ(materializeCaps<CapsMtlUB0>(), resolveCapsMtlU(AOT::MTL_U_B0));
    EXPECT_EQ(std::nullopt, resolveCapsMtlU(withUnsupportedRevision(AOT::MTL_U_A0)));
}

TEST(CapsXeLpgTest, givenMtlHIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsMtlHA0>(), resolveCapsMtlH(AOT::MTL_H_A0));
    EXPECT_EQ(materializeCaps<CapsMtlHB0>(), resolveCapsMtlH(AOT::MTL_H_B0));
    EXPECT_EQ(std::nullopt, resolveCapsMtlH(withUnsupportedRevision(AOT::MTL_H_A0)));
}

TEST(CapsXeLpgTest, givenArlHIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsArlH>(), resolveCapsArlH(AOT::ARL_H_A0));
    EXPECT_EQ(materializeCaps<CapsArlH>(), resolveCapsArlH(AOT::ARL_H_B0));
    EXPECT_EQ(std::nullopt, resolveCapsArlH(withUnsupportedRevision(AOT::ARL_H_A0)));
}

TEST(CapsXeLpgTest, givenMtlUReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsMtlUA0 = materializeCaps<CapsMtlUA0>();
    constexpr auto capsMtlUB0 = materializeCaps<CapsMtlUB0>();

    EXPECT_FALSE(capsMtlUA0.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsMtlUA0.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsMtlUA0.availableSemaphore64);
    EXPECT_TRUE(capsMtlUA0.bFloat16ConversionSupported);
    EXPECT_FALSE(capsMtlUA0.bindlessAddressingDisabled);
    EXPECT_TRUE(capsMtlUA0.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsMtlUA0.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsMtlUA0.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_TRUE(capsMtlUA0.directSubmissionLightSupported);
    EXPECT_FALSE(capsMtlUA0.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsMtlUA0.dummyBlitWaRequired);
    EXPECT_TRUE(capsMtlUA0.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsMtlUA0.latePreemptionStartSupported);
    EXPECT_TRUE(capsMtlUA0.localOnlyAllowed);
    EXPECT_TRUE(capsMtlUA0.numRtStacksPerDssFixedValue);
    EXPECT_TRUE(capsMtlUA0.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsMtlUA0.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsMtlUA0.postImageWriteFlushRequired);
    EXPECT_FALSE(capsMtlUA0.preImageReadFlushRequired);
    EXPECT_FALSE(capsMtlUA0.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_TRUE(capsMtlUA0.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsMtlUA0.queryPeerAccess);
    EXPECT_TRUE(capsMtlUA0.rayTracingSupported);
    EXPECT_FALSE(capsMtlUA0.rcsExposureDisabled);
    EXPECT_FALSE(capsMtlUA0.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsMtlUA0.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsMtlUA0.splitMatrixMultiplyAccumulateSupported);

    EXPECT_FALSE(capsMtlUB0.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsMtlUB0.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsMtlUB0.availableSemaphore64);
    EXPECT_TRUE(capsMtlUB0.bFloat16ConversionSupported);
    EXPECT_FALSE(capsMtlUB0.bindlessAddressingDisabled);
    EXPECT_TRUE(capsMtlUB0.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsMtlUB0.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsMtlUB0.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_TRUE(capsMtlUB0.directSubmissionLightSupported);
    EXPECT_FALSE(capsMtlUB0.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsMtlUB0.dummyBlitWaRequired);
    EXPECT_TRUE(capsMtlUB0.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsMtlUB0.latePreemptionStartSupported);
    EXPECT_TRUE(capsMtlUB0.localOnlyAllowed);
    EXPECT_TRUE(capsMtlUB0.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsMtlUB0.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsMtlUB0.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsMtlUB0.postImageWriteFlushRequired);
    EXPECT_FALSE(capsMtlUB0.preImageReadFlushRequired);
    EXPECT_FALSE(capsMtlUB0.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsMtlUB0.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsMtlUB0.queryPeerAccess);
    EXPECT_TRUE(capsMtlUB0.rayTracingSupported);
    EXPECT_FALSE(capsMtlUB0.rcsExposureDisabled);
    EXPECT_FALSE(capsMtlUB0.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsMtlUB0.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsMtlUB0.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXeLpgTest, givenMtlHReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsMtlHA0 = materializeCaps<CapsMtlHA0>();
    constexpr auto capsMtlHB0 = materializeCaps<CapsMtlHB0>();

    EXPECT_FALSE(capsMtlHA0.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsMtlHA0.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsMtlHA0.availableSemaphore64);
    EXPECT_TRUE(capsMtlHA0.bFloat16ConversionSupported);
    EXPECT_FALSE(capsMtlHA0.bindlessAddressingDisabled);
    EXPECT_TRUE(capsMtlHA0.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsMtlHA0.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsMtlHA0.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_TRUE(capsMtlHA0.directSubmissionLightSupported);
    EXPECT_FALSE(capsMtlHA0.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsMtlHA0.dummyBlitWaRequired);
    EXPECT_TRUE(capsMtlHA0.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsMtlHA0.latePreemptionStartSupported);
    EXPECT_TRUE(capsMtlHA0.localOnlyAllowed);
    EXPECT_TRUE(capsMtlHA0.numRtStacksPerDssFixedValue);
    EXPECT_TRUE(capsMtlHA0.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsMtlHA0.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsMtlHA0.postImageWriteFlushRequired);
    EXPECT_FALSE(capsMtlHA0.preImageReadFlushRequired);
    EXPECT_FALSE(capsMtlHA0.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_TRUE(capsMtlHA0.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsMtlHA0.queryPeerAccess);
    EXPECT_TRUE(capsMtlHA0.rayTracingSupported);
    EXPECT_FALSE(capsMtlHA0.rcsExposureDisabled);
    EXPECT_FALSE(capsMtlHA0.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsMtlHA0.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsMtlHA0.splitMatrixMultiplyAccumulateSupported);

    EXPECT_FALSE(capsMtlHB0.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsMtlHB0.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsMtlHB0.availableSemaphore64);
    EXPECT_TRUE(capsMtlHB0.bFloat16ConversionSupported);
    EXPECT_FALSE(capsMtlHB0.bindlessAddressingDisabled);
    EXPECT_TRUE(capsMtlHB0.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsMtlHB0.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsMtlHB0.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_TRUE(capsMtlHB0.directSubmissionLightSupported);
    EXPECT_FALSE(capsMtlHB0.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsMtlHB0.dummyBlitWaRequired);
    EXPECT_TRUE(capsMtlHB0.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsMtlHB0.latePreemptionStartSupported);
    EXPECT_TRUE(capsMtlHB0.localOnlyAllowed);
    EXPECT_TRUE(capsMtlHB0.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsMtlHB0.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsMtlHB0.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsMtlHB0.postImageWriteFlushRequired);
    EXPECT_FALSE(capsMtlHB0.preImageReadFlushRequired);
    EXPECT_FALSE(capsMtlHB0.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsMtlHB0.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsMtlHB0.queryPeerAccess);
    EXPECT_TRUE(capsMtlHB0.rayTracingSupported);
    EXPECT_FALSE(capsMtlHB0.rcsExposureDisabled);
    EXPECT_FALSE(capsMtlHB0.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsMtlHB0.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsMtlHB0.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXeLpgTest, givenArlHReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsArlH = materializeCaps<CapsArlH>();

    EXPECT_TRUE(capsArlH.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsArlH.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsArlH.availableSemaphore64);
    EXPECT_TRUE(capsArlH.bFloat16ConversionSupported);
    EXPECT_FALSE(capsArlH.bindlessAddressingDisabled);
    EXPECT_FALSE(capsArlH.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsArlH.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsArlH.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_TRUE(capsArlH.directSubmissionLightSupported);
    EXPECT_TRUE(capsArlH.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsArlH.dummyBlitWaRequired);
    EXPECT_TRUE(capsArlH.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsArlH.latePreemptionStartSupported);
    EXPECT_TRUE(capsArlH.localOnlyAllowed);
    EXPECT_TRUE(capsArlH.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsArlH.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_TRUE(capsArlH.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsArlH.postImageWriteFlushRequired);
    EXPECT_FALSE(capsArlH.preImageReadFlushRequired);
    EXPECT_FALSE(capsArlH.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsArlH.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsArlH.queryPeerAccess);
    EXPECT_TRUE(capsArlH.rayTracingSupported);
    EXPECT_FALSE(capsArlH.rcsExposureDisabled);
    EXPECT_FALSE(capsArlH.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsArlH.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsArlH.splitMatrixMultiplyAccumulateSupported);
}
