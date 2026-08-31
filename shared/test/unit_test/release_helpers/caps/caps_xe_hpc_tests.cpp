/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/caps/caps_xe_hpc.h"

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

TEST(CapsXeHpcTest, givenPvcIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsPvc>(), resolveCapsPvc(AOT::PVC_XL_A0));
    EXPECT_EQ(materializeCaps<CapsPvc>(), resolveCapsPvc(AOT::PVC_XL_A0P));
    EXPECT_EQ(materializeCaps<CapsPvc>(), resolveCapsPvc(AOT::PVC_XT_A0));
    EXPECT_EQ(materializeCaps<CapsPvc>(), resolveCapsPvc(AOT::PVC_XT_B0));
    EXPECT_EQ(materializeCaps<CapsPvc>(), resolveCapsPvc(AOT::PVC_XT_B1));
    EXPECT_EQ(materializeCaps<CapsPvc>(), resolveCapsPvc(AOT::PVC_XT_C0));
    EXPECT_EQ(std::nullopt, resolveCapsPvc(withUnsupportedRevision(AOT::PVC_XL_A0)));
}

TEST(CapsXeHpcTest, givenPvcVgIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsPvcVg>(), resolveCapsPvcVg(AOT::PVC_XT_C0_VG));
    EXPECT_EQ(std::nullopt, resolveCapsPvcVg(withUnsupportedRevision(AOT::PVC_XT_C0_VG)));
}

TEST(CapsXeHpcTest, givenPvcReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsPvc = materializeCaps<CapsPvc>();
    EXPECT_FALSE(capsPvc.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsPvc.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsPvc.availableSemaphore64);
    EXPECT_TRUE(capsPvc.bFloat16ConversionSupported);
    EXPECT_TRUE(capsPvc.bindlessAddressingDisabled);
    EXPECT_FALSE(capsPvc.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsPvc.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsPvc.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsPvc.directSubmissionLightSupported);
    EXPECT_TRUE(capsPvc.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsPvc.dummyBlitWaRequired);
    EXPECT_FALSE(capsPvc.forceEmuInt32DivRemSPRequired);
    EXPECT_FALSE(capsPvc.ftrXe2Compression);
    EXPECT_FALSE(capsPvc.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsPvc.latePreemptionStartSupported);
    EXPECT_TRUE(capsPvc.localOnlyAllowed);
    EXPECT_TRUE(capsPvc.matrixMultiplyAccumulateSupported);
    EXPECT_TRUE(capsPvc.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsPvc.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsPvc.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsPvc.postImageWriteFlushRequired);
    EXPECT_FALSE(capsPvc.preImageReadFlushRequired);
    EXPECT_FALSE(capsPvc.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsPvc.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsPvc.queryPeerAccess);
    EXPECT_TRUE(capsPvc.rayTracingSupported);
    EXPECT_TRUE(capsPvc.rcsExposureDisabled);
    EXPECT_FALSE(capsPvc.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsPvc.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsPvc.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXeHpcTest, givenPvcVgReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsPvcVg = materializeCaps<CapsPvcVg>();
    EXPECT_FALSE(capsPvcVg.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsPvcVg.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsPvcVg.availableSemaphore64);
    EXPECT_TRUE(capsPvcVg.bFloat16ConversionSupported);
    EXPECT_TRUE(capsPvcVg.bindlessAddressingDisabled);
    EXPECT_FALSE(capsPvcVg.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsPvcVg.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsPvcVg.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsPvcVg.directSubmissionLightSupported);
    EXPECT_FALSE(capsPvcVg.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsPvcVg.dummyBlitWaRequired);
    EXPECT_FALSE(capsPvcVg.forceEmuInt32DivRemSPRequired);
    EXPECT_FALSE(capsPvcVg.ftrXe2Compression);
    EXPECT_FALSE(capsPvcVg.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsPvcVg.latePreemptionStartSupported);
    EXPECT_TRUE(capsPvcVg.localOnlyAllowed);
    EXPECT_FALSE(capsPvcVg.matrixMultiplyAccumulateSupported);
    EXPECT_TRUE(capsPvcVg.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsPvcVg.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsPvcVg.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsPvcVg.postImageWriteFlushRequired);
    EXPECT_FALSE(capsPvcVg.preImageReadFlushRequired);
    EXPECT_FALSE(capsPvcVg.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsPvcVg.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsPvcVg.queryPeerAccess);
    EXPECT_TRUE(capsPvcVg.rayTracingSupported);
    EXPECT_TRUE(capsPvcVg.rcsExposureDisabled);
    EXPECT_FALSE(capsPvcVg.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsPvcVg.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsPvcVg.splitMatrixMultiplyAccumulateSupported);
}
