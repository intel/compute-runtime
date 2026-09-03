/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/caps/caps_gen12lp.h"

#include "gtest/gtest.h"
#include "neo_aot_platforms.h"

using namespace NEO;

TEST(CapsGen12LpTest, givenGen12LpIpVersionWhenResolvingCapsThenCapsOfMatchingReleaseAreReturned) {
    EXPECT_EQ(materializeCaps<CapsTgl>(), resolveCapsTgl(AOT::TGL));
    EXPECT_EQ(materializeCaps<CapsRkl>(), resolveCapsRkl(AOT::RKL));
    EXPECT_EQ(materializeCaps<CapsAdlS>(), resolveCapsAdlS(AOT::ADL_S));
    EXPECT_EQ(materializeCaps<CapsAdlP>(), resolveCapsAdlP(AOT::ADL_P));
    EXPECT_EQ(materializeCaps<CapsAdlN>(), resolveCapsAdlN(AOT::ADL_N));
    EXPECT_EQ(materializeCaps<CapsDg1>(), resolveCapsDg1(AOT::DG1));
}

TEST(CapsGen12LpTest, givenTglReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsTgl = materializeCaps<CapsTgl>();
    EXPECT_EQ(0u, capsTgl.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, capsTgl.kernelFp16AtomicCapabilities);
    EXPECT_FALSE(capsTgl.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsTgl.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsTgl.availableSemaphore64);
    EXPECT_TRUE(capsTgl.bFloat16ConversionSupported);
    EXPECT_TRUE(capsTgl.bindlessAddressingDisabled);
    EXPECT_FALSE(capsTgl.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsTgl.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsTgl.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsTgl.directSubmissionLightSupported);
    EXPECT_FALSE(capsTgl.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsTgl.dummyBlitWaRequired);
    EXPECT_FALSE(capsTgl.forceEmuInt32DivRemSPRequired);
    EXPECT_FALSE(capsTgl.ftrXe2Compression);
    EXPECT_FALSE(capsTgl.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsTgl.latePreemptionStartSupported);
    EXPECT_TRUE(capsTgl.localOnlyAllowed);
    EXPECT_FALSE(capsTgl.matrixMultiplyAccumulateSupported);
    EXPECT_TRUE(capsTgl.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsTgl.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsTgl.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsTgl.postImageWriteFlushRequired);
    EXPECT_FALSE(capsTgl.preImageReadFlushRequired);
    EXPECT_FALSE(capsTgl.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsTgl.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsTgl.queryPeerAccess);
    EXPECT_FALSE(capsTgl.rayTracingSupported);
    EXPECT_FALSE(capsTgl.rcsExposureDisabled);
    EXPECT_FALSE(capsTgl.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsTgl.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsTgl.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsGen12LpTest, givenRklReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsRkl = materializeCaps<CapsRkl>();
    EXPECT_EQ(0u, capsRkl.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, capsRkl.kernelFp16AtomicCapabilities);
    EXPECT_FALSE(capsRkl.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsRkl.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsRkl.availableSemaphore64);
    EXPECT_TRUE(capsRkl.bFloat16ConversionSupported);
    EXPECT_TRUE(capsRkl.bindlessAddressingDisabled);
    EXPECT_FALSE(capsRkl.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsRkl.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsRkl.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsRkl.directSubmissionLightSupported);
    EXPECT_FALSE(capsRkl.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsRkl.dummyBlitWaRequired);
    EXPECT_TRUE(capsRkl.forceEmuInt32DivRemSPRequired);
    EXPECT_FALSE(capsRkl.ftrXe2Compression);
    EXPECT_FALSE(capsRkl.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsRkl.latePreemptionStartSupported);
    EXPECT_TRUE(capsRkl.localOnlyAllowed);
    EXPECT_FALSE(capsRkl.matrixMultiplyAccumulateSupported);
    EXPECT_TRUE(capsRkl.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsRkl.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsRkl.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsRkl.postImageWriteFlushRequired);
    EXPECT_FALSE(capsRkl.preImageReadFlushRequired);
    EXPECT_FALSE(capsRkl.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsRkl.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsRkl.queryPeerAccess);
    EXPECT_FALSE(capsRkl.rayTracingSupported);
    EXPECT_FALSE(capsRkl.rcsExposureDisabled);
    EXPECT_FALSE(capsRkl.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsRkl.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsRkl.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsGen12LpTest, givenAdlSReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsAdlS = materializeCaps<CapsAdlS>();
    EXPECT_EQ(0u, capsAdlS.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, capsAdlS.kernelFp16AtomicCapabilities);
    EXPECT_FALSE(capsAdlS.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsAdlS.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsAdlS.availableSemaphore64);
    EXPECT_TRUE(capsAdlS.bFloat16ConversionSupported);
    EXPECT_TRUE(capsAdlS.bindlessAddressingDisabled);
    EXPECT_FALSE(capsAdlS.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsAdlS.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsAdlS.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsAdlS.directSubmissionLightSupported);
    EXPECT_FALSE(capsAdlS.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsAdlS.dummyBlitWaRequired);
    EXPECT_FALSE(capsAdlS.forceEmuInt32DivRemSPRequired);
    EXPECT_FALSE(capsAdlS.ftrXe2Compression);
    EXPECT_FALSE(capsAdlS.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsAdlS.latePreemptionStartSupported);
    EXPECT_TRUE(capsAdlS.localOnlyAllowed);
    EXPECT_FALSE(capsAdlS.matrixMultiplyAccumulateSupported);
    EXPECT_TRUE(capsAdlS.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsAdlS.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsAdlS.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsAdlS.postImageWriteFlushRequired);
    EXPECT_FALSE(capsAdlS.preImageReadFlushRequired);
    EXPECT_FALSE(capsAdlS.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsAdlS.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsAdlS.queryPeerAccess);
    EXPECT_FALSE(capsAdlS.rayTracingSupported);
    EXPECT_FALSE(capsAdlS.rcsExposureDisabled);
    EXPECT_FALSE(capsAdlS.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsAdlS.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsAdlS.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsGen12LpTest, givenAdlPReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsAdlP = materializeCaps<CapsAdlP>();
    EXPECT_EQ(0u, capsAdlP.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, capsAdlP.kernelFp16AtomicCapabilities);
    EXPECT_FALSE(capsAdlP.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsAdlP.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsAdlP.availableSemaphore64);
    EXPECT_TRUE(capsAdlP.bFloat16ConversionSupported);
    EXPECT_TRUE(capsAdlP.bindlessAddressingDisabled);
    EXPECT_FALSE(capsAdlP.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsAdlP.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsAdlP.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsAdlP.directSubmissionLightSupported);
    EXPECT_FALSE(capsAdlP.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsAdlP.dummyBlitWaRequired);
    EXPECT_FALSE(capsAdlP.forceEmuInt32DivRemSPRequired);
    EXPECT_FALSE(capsAdlP.ftrXe2Compression);
    EXPECT_FALSE(capsAdlP.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsAdlP.latePreemptionStartSupported);
    EXPECT_TRUE(capsAdlP.localOnlyAllowed);
    EXPECT_FALSE(capsAdlP.matrixMultiplyAccumulateSupported);
    EXPECT_TRUE(capsAdlP.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsAdlP.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsAdlP.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsAdlP.postImageWriteFlushRequired);
    EXPECT_FALSE(capsAdlP.preImageReadFlushRequired);
    EXPECT_FALSE(capsAdlP.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsAdlP.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsAdlP.queryPeerAccess);
    EXPECT_FALSE(capsAdlP.rayTracingSupported);
    EXPECT_FALSE(capsAdlP.rcsExposureDisabled);
    EXPECT_FALSE(capsAdlP.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsAdlP.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsAdlP.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsGen12LpTest, givenAdlNReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsAdlN = materializeCaps<CapsAdlN>();
    EXPECT_EQ(0u, capsAdlN.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, capsAdlN.kernelFp16AtomicCapabilities);
    EXPECT_FALSE(capsAdlN.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsAdlN.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsAdlN.availableSemaphore64);
    EXPECT_TRUE(capsAdlN.bFloat16ConversionSupported);
    EXPECT_TRUE(capsAdlN.bindlessAddressingDisabled);
    EXPECT_FALSE(capsAdlN.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsAdlN.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsAdlN.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsAdlN.directSubmissionLightSupported);
    EXPECT_FALSE(capsAdlN.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsAdlN.dummyBlitWaRequired);
    EXPECT_FALSE(capsAdlN.forceEmuInt32DivRemSPRequired);
    EXPECT_FALSE(capsAdlN.ftrXe2Compression);
    EXPECT_FALSE(capsAdlN.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsAdlN.latePreemptionStartSupported);
    EXPECT_TRUE(capsAdlN.localOnlyAllowed);
    EXPECT_FALSE(capsAdlN.matrixMultiplyAccumulateSupported);
    EXPECT_TRUE(capsAdlN.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsAdlN.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsAdlN.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsAdlN.postImageWriteFlushRequired);
    EXPECT_FALSE(capsAdlN.preImageReadFlushRequired);
    EXPECT_FALSE(capsAdlN.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsAdlN.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsAdlN.queryPeerAccess);
    EXPECT_FALSE(capsAdlN.rayTracingSupported);
    EXPECT_FALSE(capsAdlN.rcsExposureDisabled);
    EXPECT_FALSE(capsAdlN.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsAdlN.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsAdlN.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsGen12LpTest, givenDg1ReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsDg1 = materializeCaps<CapsDg1>();
    EXPECT_EQ(0u, capsDg1.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, capsDg1.kernelFp16AtomicCapabilities);
    EXPECT_FALSE(capsDg1.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsDg1.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsDg1.availableSemaphore64);
    EXPECT_TRUE(capsDg1.bFloat16ConversionSupported);
    EXPECT_TRUE(capsDg1.bindlessAddressingDisabled);
    EXPECT_FALSE(capsDg1.blitImageAllowedForDepthFormat);
    EXPECT_FALSE(capsDg1.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsDg1.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsDg1.directSubmissionLightSupported);
    EXPECT_FALSE(capsDg1.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsDg1.dummyBlitWaRequired);
    EXPECT_FALSE(capsDg1.forceEmuInt32DivRemSPRequired);
    EXPECT_FALSE(capsDg1.ftrXe2Compression);
    EXPECT_FALSE(capsDg1.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsDg1.latePreemptionStartSupported);
    EXPECT_TRUE(capsDg1.localOnlyAllowed);
    EXPECT_FALSE(capsDg1.matrixMultiplyAccumulateSupported);
    EXPECT_TRUE(capsDg1.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsDg1.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsDg1.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsDg1.postImageWriteFlushRequired);
    EXPECT_FALSE(capsDg1.preImageReadFlushRequired);
    EXPECT_FALSE(capsDg1.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsDg1.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsDg1.queryPeerAccess);
    EXPECT_FALSE(capsDg1.rayTracingSupported);
    EXPECT_FALSE(capsDg1.rcsExposureDisabled);
    EXPECT_FALSE(capsDg1.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsDg1.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsDg1.splitMatrixMultiplyAccumulateSupported);
}
