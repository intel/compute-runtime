/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/caps/caps_xe3.h"

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

TEST(CapsXe3Test, givenPtlHIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsPtlHA0>(), resolveCapsPtlH(AOT::PTL_H_A0));
    EXPECT_EQ(materializeCaps<CapsPtlHB0>(), resolveCapsPtlH(AOT::PTL_H_B0));
    EXPECT_EQ(std::nullopt, resolveCapsPtlH(withUnsupportedRevision(AOT::PTL_H_A0)));
}

TEST(CapsXe3Test, givenPtlUIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsPtlU>(), resolveCapsPtlU(AOT::PTL_U_A0));
    EXPECT_EQ(std::nullopt, resolveCapsPtlU(withUnsupportedRevision(AOT::PTL_U_A0)));
}

TEST(CapsXe3Test, givenWclIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsWcl>(), resolveCapsWcl(AOT::WCL_A0));
    EXPECT_EQ(materializeCaps<CapsWcl>(), resolveCapsWcl(AOT::WCL_A1));
    EXPECT_EQ(std::nullopt, resolveCapsWcl(withUnsupportedRevision(AOT::WCL_A0)));
}

TEST(CapsXe3Test, givenNvlSIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsNvlS>(), resolveCapsNvlS(AOT::NVL_S_A0));
    EXPECT_EQ(materializeCaps<CapsNvlS>(), resolveCapsNvlS(AOT::NVL_S_B0));
    EXPECT_EQ(std::nullopt, resolveCapsNvlS(withUnsupportedRevision(AOT::NVL_S_A0)));
}

TEST(CapsXe3Test, givenNvlUIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsNvlU>(), resolveCapsNvlU(AOT::NVL_U_A0));
    EXPECT_EQ(materializeCaps<CapsNvlU>(), resolveCapsNvlU(AOT::NVL_U_A1));
    EXPECT_EQ(materializeCaps<CapsNvlU>(), resolveCapsNvlU(AOT::NVL_U_B0));
    EXPECT_EQ(std::nullopt, resolveCapsNvlU(withUnsupportedRevision(AOT::NVL_U_A0)));
}

TEST(CapsXe3Test, givenPtlHReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsPtlHA0 = materializeCaps<CapsPtlHA0>();
    constexpr auto capsPtlHB0 = materializeCaps<CapsPtlHB0>();

    EXPECT_EQ(256u, capsPtlHA0.cacheLineSize);
    EXPECT_EQ(2u, capsPtlHA0.commandBuffersPreallocatedPerCommandQueue);
    EXPECT_EQ(0u, capsPtlHA0.defaultMidthreadPreemptionDelayTimer);
    EXPECT_EQ(0u, capsPtlHA0.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, capsPtlHA0.kernelFp16AtomicCapabilities);
    EXPECT_EQ(16u, capsPtlHA0.maxNumSamplers);
    EXPECT_EQ(16128u, capsPtlHA0.planarYuvMaxHeight);
    EXPECT_EQ(0u, capsPtlHA0.preferredWorkgroupCountPerSubslice);
    EXPECT_EQ(2u, capsPtlHA0.rtasFormat);
    EXPECT_EQ(64u, capsPtlHA0.stackSizePerRay);
    EXPECT_EQ(16384u, capsPtlHA0.max3dImageWidthOrHeight);
    EXPECT_EQ(MemoryConstants::pageSize64k, capsPtlHA0.svmCpuAlignment);
    EXPECT_FALSE(capsPtlHA0.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsPtlHA0.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsPtlHA0.availableSemaphore64);
    EXPECT_TRUE(capsPtlHA0.bFloat16ConversionSupported);
    EXPECT_FALSE(capsPtlHA0.bindlessAddressingDisabled);
    EXPECT_TRUE(capsPtlHA0.blitImageAllowedForDepthFormat);
    EXPECT_TRUE(capsPtlHA0.block2DLoadSupported);
    EXPECT_TRUE(capsPtlHB0.block2DLoadSupported);
    EXPECT_TRUE(capsPtlHA0.block2DStoreSupported);
    EXPECT_TRUE(capsPtlHB0.block2DStoreSupported);
    EXPECT_TRUE(capsPtlHA0.cacheFlushPriorToImageReadRequired);
    EXPECT_TRUE(capsPtlHA0.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsPtlHA0.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsPtlHA0.directSubmissionLightSupported);
    EXPECT_TRUE(capsPtlHA0.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsPtlHA0.dummyBlitWaRequired);
    EXPECT_FALSE(capsPtlHA0.forceEmuInt32DivRemSPRequired);
    EXPECT_FALSE(capsPtlHA0.ftrXe2Compression);
    EXPECT_TRUE(capsPtlHA0.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsPtlHA0.hvAlign4Required);
    EXPECT_FALSE(capsPtlHA0.initBuiltinAsyncSupported);
    EXPECT_FALSE(capsPtlHA0.initDeviceWithFirstSubmissionRequired);
    EXPECT_FALSE(capsPtlHA0.latePreemptionStartSupported);
    EXPECT_FALSE(capsPtlHA0.localOnlyAllowed);
    EXPECT_TRUE(capsPtlHA0.matrixMultiplyAccumulateSupported);
    EXPECT_FALSE(capsPtlHA0.memSetExtendedPayloadSupported);
    EXPECT_FALSE(capsPtlHB0.memSetExtendedPayloadSupported);
    EXPECT_TRUE(capsPtlHA0.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsPtlHA0.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsPtlHA0.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsPtlHA0.postImageWriteFlushRequired);
    EXPECT_TRUE(capsPtlHA0.preImageReadFlushRequired);
    EXPECT_FALSE(capsPtlHA0.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsPtlHA0.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsPtlHA0.queryPeerAccess);
    EXPECT_TRUE(capsPtlHA0.rayTracingSupported);
    EXPECT_FALSE(capsPtlHA0.rayTracingWalkerAdjustmentRequired);
    EXPECT_TRUE(capsPtlHA0.rcsExposureDisabled);
    EXPECT_FALSE(capsPtlHA0.reducedSurfaceStateSupported);
    EXPECT_TRUE(capsPtlHA0.scratchSpaceBasePointerInGrf);
    EXPECT_TRUE(capsPtlHB0.scratchSpaceBasePointerInGrf);
    EXPECT_FALSE(capsPtlHA0.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsPtlHA0.splitMatrixMultiplyAccumulateSupported);

    EXPECT_EQ(256u, capsPtlHB0.cacheLineSize);
    EXPECT_EQ(2u, capsPtlHB0.commandBuffersPreallocatedPerCommandQueue);
    EXPECT_EQ(0u, capsPtlHB0.defaultMidthreadPreemptionDelayTimer);
    EXPECT_EQ(0u, capsPtlHB0.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, capsPtlHB0.kernelFp16AtomicCapabilities);
    EXPECT_EQ(16u, capsPtlHB0.maxNumSamplers);
    EXPECT_EQ(16128u, capsPtlHB0.planarYuvMaxHeight);
    EXPECT_EQ(0u, capsPtlHB0.preferredWorkgroupCountPerSubslice);
    EXPECT_EQ(2u, capsPtlHB0.rtasFormat);
    EXPECT_EQ(64u, capsPtlHB0.stackSizePerRay);
    EXPECT_EQ(16384u, capsPtlHB0.max3dImageWidthOrHeight);
    EXPECT_EQ(MemoryConstants::pageSize64k, capsPtlHB0.svmCpuAlignment);
    EXPECT_FALSE(capsPtlHB0.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsPtlHB0.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsPtlHB0.availableSemaphore64);
    EXPECT_TRUE(capsPtlHB0.bFloat16ConversionSupported);
    EXPECT_FALSE(capsPtlHB0.bindlessAddressingDisabled);
    EXPECT_TRUE(capsPtlHB0.blitImageAllowedForDepthFormat);
    EXPECT_TRUE(capsPtlHB0.cacheFlushPriorToImageReadRequired);
    EXPECT_TRUE(capsPtlHB0.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsPtlHB0.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsPtlHB0.directSubmissionLightSupported);
    EXPECT_TRUE(capsPtlHB0.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsPtlHB0.dummyBlitWaRequired);
    EXPECT_FALSE(capsPtlHB0.forceEmuInt32DivRemSPRequired);
    EXPECT_TRUE(capsPtlHB0.ftrXe2Compression);
    EXPECT_TRUE(capsPtlHB0.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsPtlHB0.hvAlign4Required);
    EXPECT_FALSE(capsPtlHB0.initBuiltinAsyncSupported);
    EXPECT_FALSE(capsPtlHB0.initDeviceWithFirstSubmissionRequired);
    EXPECT_FALSE(capsPtlHB0.latePreemptionStartSupported);
    EXPECT_FALSE(capsPtlHB0.localOnlyAllowed);
    EXPECT_TRUE(capsPtlHB0.matrixMultiplyAccumulateSupported);
    EXPECT_TRUE(capsPtlHB0.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsPtlHB0.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsPtlHB0.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsPtlHB0.postImageWriteFlushRequired);
    EXPECT_TRUE(capsPtlHB0.preImageReadFlushRequired);
    EXPECT_FALSE(capsPtlHB0.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsPtlHB0.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsPtlHB0.queryPeerAccess);
    EXPECT_TRUE(capsPtlHB0.rayTracingSupported);
    EXPECT_FALSE(capsPtlHB0.rayTracingWalkerAdjustmentRequired);
    EXPECT_TRUE(capsPtlHB0.rcsExposureDisabled);
    EXPECT_FALSE(capsPtlHB0.reducedSurfaceStateSupported);
    EXPECT_FALSE(capsPtlHB0.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsPtlHB0.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3Test, givenPtlUReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsPtlU = materializeCaps<CapsPtlU>();

    EXPECT_EQ(256u, capsPtlU.cacheLineSize);
    EXPECT_EQ(2u, capsPtlU.commandBuffersPreallocatedPerCommandQueue);
    EXPECT_EQ(0u, capsPtlU.defaultMidthreadPreemptionDelayTimer);
    EXPECT_EQ(0u, capsPtlU.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, capsPtlU.kernelFp16AtomicCapabilities);
    EXPECT_EQ(16u, capsPtlU.maxNumSamplers);
    EXPECT_EQ(16128u, capsPtlU.planarYuvMaxHeight);
    EXPECT_EQ(0u, capsPtlU.preferredWorkgroupCountPerSubslice);
    EXPECT_EQ(2u, capsPtlU.rtasFormat);
    EXPECT_EQ(64u, capsPtlU.stackSizePerRay);
    EXPECT_EQ(16384u, capsPtlU.max3dImageWidthOrHeight);
    EXPECT_EQ(MemoryConstants::pageSize64k, capsPtlU.svmCpuAlignment);
    EXPECT_FALSE(capsPtlU.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsPtlU.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsPtlU.availableSemaphore64);
    EXPECT_TRUE(capsPtlU.bFloat16ConversionSupported);
    EXPECT_FALSE(capsPtlU.bindlessAddressingDisabled);
    EXPECT_TRUE(capsPtlU.blitImageAllowedForDepthFormat);
    EXPECT_TRUE(capsPtlU.block2DLoadSupported);
    EXPECT_TRUE(capsPtlU.block2DStoreSupported);
    EXPECT_TRUE(capsPtlU.cacheFlushPriorToImageReadRequired);
    EXPECT_TRUE(capsPtlU.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsPtlU.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsPtlU.directSubmissionLightSupported);
    EXPECT_TRUE(capsPtlU.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsPtlU.dummyBlitWaRequired);
    EXPECT_FALSE(capsPtlU.forceEmuInt32DivRemSPRequired);
    EXPECT_TRUE(capsPtlU.ftrXe2Compression);
    EXPECT_TRUE(capsPtlU.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsPtlU.hvAlign4Required);
    EXPECT_FALSE(capsPtlU.initBuiltinAsyncSupported);
    EXPECT_FALSE(capsPtlU.initDeviceWithFirstSubmissionRequired);
    EXPECT_FALSE(capsPtlU.latePreemptionStartSupported);
    EXPECT_FALSE(capsPtlU.localOnlyAllowed);
    EXPECT_TRUE(capsPtlU.matrixMultiplyAccumulateSupported);
    EXPECT_FALSE(capsPtlU.memSetExtendedPayloadSupported);
    EXPECT_TRUE(capsPtlU.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsPtlU.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsPtlU.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsPtlU.postImageWriteFlushRequired);
    EXPECT_TRUE(capsPtlU.preImageReadFlushRequired);
    EXPECT_FALSE(capsPtlU.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsPtlU.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsPtlU.queryPeerAccess);
    EXPECT_TRUE(capsPtlU.rayTracingSupported);
    EXPECT_FALSE(capsPtlU.rayTracingWalkerAdjustmentRequired);
    EXPECT_TRUE(capsPtlU.rcsExposureDisabled);
    EXPECT_FALSE(capsPtlU.reducedSurfaceStateSupported);
    EXPECT_TRUE(capsPtlU.scratchSpaceBasePointerInGrf);
    EXPECT_FALSE(capsPtlU.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsPtlU.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3Test, givenWclReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsWcl = materializeCaps<CapsWcl>();

    EXPECT_EQ(256u, capsWcl.cacheLineSize);
    EXPECT_EQ(2u, capsWcl.commandBuffersPreallocatedPerCommandQueue);
    EXPECT_EQ(0u, capsWcl.defaultMidthreadPreemptionDelayTimer);
    EXPECT_EQ(0u, capsWcl.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, capsWcl.kernelFp16AtomicCapabilities);
    EXPECT_EQ(16u, capsWcl.maxNumSamplers);
    EXPECT_EQ(16128u, capsWcl.planarYuvMaxHeight);
    EXPECT_EQ(0u, capsWcl.preferredWorkgroupCountPerSubslice);
    EXPECT_EQ(2u, capsWcl.rtasFormat);
    EXPECT_EQ(64u, capsWcl.stackSizePerRay);
    EXPECT_EQ(16384u, capsWcl.max3dImageWidthOrHeight);
    EXPECT_EQ(MemoryConstants::pageSize64k, capsWcl.svmCpuAlignment);
    EXPECT_FALSE(capsWcl.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsWcl.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsWcl.availableSemaphore64);
    EXPECT_TRUE(capsWcl.bFloat16ConversionSupported);
    EXPECT_FALSE(capsWcl.bindlessAddressingDisabled);
    EXPECT_TRUE(capsWcl.blitImageAllowedForDepthFormat);
    EXPECT_TRUE(capsWcl.block2DLoadSupported);
    EXPECT_TRUE(capsWcl.block2DStoreSupported);
    EXPECT_TRUE(capsWcl.cacheFlushPriorToImageReadRequired);
    EXPECT_TRUE(capsWcl.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsWcl.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsWcl.directSubmissionLightSupported);
    EXPECT_TRUE(capsWcl.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsWcl.dummyBlitWaRequired);
    EXPECT_FALSE(capsWcl.forceEmuInt32DivRemSPRequired);
    EXPECT_TRUE(capsWcl.ftrXe2Compression);
    EXPECT_TRUE(capsWcl.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsWcl.hvAlign4Required);
    EXPECT_FALSE(capsWcl.initBuiltinAsyncSupported);
    EXPECT_FALSE(capsWcl.initDeviceWithFirstSubmissionRequired);
    EXPECT_FALSE(capsWcl.latePreemptionStartSupported);
    EXPECT_FALSE(capsWcl.localOnlyAllowed);
    EXPECT_TRUE(capsWcl.matrixMultiplyAccumulateSupported);
    EXPECT_FALSE(capsWcl.memSetExtendedPayloadSupported);
    EXPECT_TRUE(capsWcl.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsWcl.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsWcl.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsWcl.postImageWriteFlushRequired);
    EXPECT_TRUE(capsWcl.preImageReadFlushRequired);
    EXPECT_FALSE(capsWcl.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsWcl.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsWcl.queryPeerAccess);
    EXPECT_FALSE(capsWcl.rayTracingSupported);
    EXPECT_FALSE(capsWcl.rayTracingWalkerAdjustmentRequired);
    EXPECT_TRUE(capsWcl.rcsExposureDisabled);
    EXPECT_FALSE(capsWcl.reducedSurfaceStateSupported);
    EXPECT_TRUE(capsWcl.scratchSpaceBasePointerInGrf);
    EXPECT_FALSE(capsWcl.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsWcl.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3Test, givenNvlSReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsNvlS = materializeCaps<CapsNvlS>();

    EXPECT_EQ(256u, capsNvlS.cacheLineSize);
    EXPECT_EQ(2u, capsNvlS.commandBuffersPreallocatedPerCommandQueue);
    EXPECT_EQ(0u, capsNvlS.defaultMidthreadPreemptionDelayTimer);
    EXPECT_EQ(0u, capsNvlS.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, capsNvlS.kernelFp16AtomicCapabilities);
    EXPECT_EQ(16u, capsNvlS.maxNumSamplers);
    EXPECT_EQ(16128u, capsNvlS.planarYuvMaxHeight);
    EXPECT_EQ(0u, capsNvlS.preferredWorkgroupCountPerSubslice);
    EXPECT_EQ(2u, capsNvlS.rtasFormat);
    EXPECT_EQ(64u, capsNvlS.stackSizePerRay);
    EXPECT_EQ(16384u, capsNvlS.max3dImageWidthOrHeight);
    EXPECT_EQ(MemoryConstants::pageSize64k, capsNvlS.svmCpuAlignment);
    EXPECT_FALSE(capsNvlS.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsNvlS.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsNvlS.availableSemaphore64);
    EXPECT_TRUE(capsNvlS.bFloat16ConversionSupported);
    EXPECT_FALSE(capsNvlS.bindlessAddressingDisabled);
    EXPECT_TRUE(capsNvlS.blitImageAllowedForDepthFormat);
    EXPECT_TRUE(capsNvlS.block2DLoadSupported);
    EXPECT_TRUE(capsNvlS.block2DStoreSupported);
    EXPECT_TRUE(capsNvlS.cacheFlushPriorToImageReadRequired);
    EXPECT_TRUE(capsNvlS.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsNvlS.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsNvlS.directSubmissionLightSupported);
    EXPECT_TRUE(capsNvlS.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsNvlS.dummyBlitWaRequired);
    EXPECT_FALSE(capsNvlS.forceEmuInt32DivRemSPRequired);
    EXPECT_TRUE(capsNvlS.ftrXe2Compression);
    EXPECT_TRUE(capsNvlS.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsNvlS.hvAlign4Required);
    EXPECT_FALSE(capsNvlS.initBuiltinAsyncSupported);
    EXPECT_FALSE(capsNvlS.initDeviceWithFirstSubmissionRequired);
    EXPECT_FALSE(capsNvlS.latePreemptionStartSupported);
    EXPECT_FALSE(capsNvlS.localOnlyAllowed);
    EXPECT_TRUE(capsNvlS.matrixMultiplyAccumulateSupported);
    EXPECT_FALSE(capsNvlS.memSetExtendedPayloadSupported);
    EXPECT_TRUE(capsNvlS.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsNvlS.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsNvlS.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsNvlS.postImageWriteFlushRequired);
    EXPECT_TRUE(capsNvlS.preImageReadFlushRequired);
    EXPECT_FALSE(capsNvlS.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsNvlS.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsNvlS.queryPeerAccess);
    EXPECT_FALSE(capsNvlS.rayTracingSupported);
    EXPECT_FALSE(capsNvlS.rayTracingWalkerAdjustmentRequired);
    EXPECT_TRUE(capsNvlS.rcsExposureDisabled);
    EXPECT_FALSE(capsNvlS.reducedSurfaceStateSupported);
    EXPECT_TRUE(capsNvlS.scratchSpaceBasePointerInGrf);
    EXPECT_FALSE(capsNvlS.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsNvlS.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3Test, givenNvlUReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsNvlU = materializeCaps<CapsNvlU>();

    EXPECT_EQ(256u, capsNvlU.cacheLineSize);
    EXPECT_EQ(2u, capsNvlU.commandBuffersPreallocatedPerCommandQueue);
    EXPECT_EQ(0u, capsNvlU.defaultMidthreadPreemptionDelayTimer);
    EXPECT_EQ(0u, capsNvlU.kernelBFloat16AtomicCapabilities);
    EXPECT_EQ(FpAtomicExtFlags::minMaxAtomicCaps | FpAtomicExtFlags::loadStoreAtomicCaps, capsNvlU.kernelFp16AtomicCapabilities);
    EXPECT_EQ(16u, capsNvlU.maxNumSamplers);
    EXPECT_EQ(16128u, capsNvlU.planarYuvMaxHeight);
    EXPECT_EQ(0u, capsNvlU.preferredWorkgroupCountPerSubslice);
    EXPECT_EQ(2u, capsNvlU.rtasFormat);
    EXPECT_EQ(64u, capsNvlU.stackSizePerRay);
    EXPECT_EQ(16384u, capsNvlU.max3dImageWidthOrHeight);
    EXPECT_EQ(MemoryConstants::pageSize64k, capsNvlU.svmCpuAlignment);
    EXPECT_FALSE(capsNvlU.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsNvlU.auxSurfaceModeOverrideRequired);
    EXPECT_FALSE(capsNvlU.availableSemaphore64);
    EXPECT_TRUE(capsNvlU.bFloat16ConversionSupported);
    EXPECT_FALSE(capsNvlU.bindlessAddressingDisabled);
    EXPECT_TRUE(capsNvlU.blitImageAllowedForDepthFormat);
    EXPECT_TRUE(capsNvlU.block2DLoadSupported);
    EXPECT_TRUE(capsNvlU.block2DStoreSupported);
    EXPECT_TRUE(capsNvlU.cacheFlushPriorToImageReadRequired);
    EXPECT_TRUE(capsNvlU.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsNvlU.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsNvlU.directSubmissionLightSupported);
    EXPECT_TRUE(capsNvlU.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsNvlU.dummyBlitWaRequired);
    EXPECT_FALSE(capsNvlU.forceEmuInt32DivRemSPRequired);
    EXPECT_TRUE(capsNvlU.ftrXe2Compression);
    EXPECT_TRUE(capsNvlU.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsNvlU.hvAlign4Required);
    EXPECT_FALSE(capsNvlU.initBuiltinAsyncSupported);
    EXPECT_FALSE(capsNvlU.initDeviceWithFirstSubmissionRequired);
    EXPECT_FALSE(capsNvlU.latePreemptionStartSupported);
    EXPECT_FALSE(capsNvlU.localOnlyAllowed);
    EXPECT_TRUE(capsNvlU.matrixMultiplyAccumulateSupported);
    EXPECT_FALSE(capsNvlU.memSetExtendedPayloadSupported);
    EXPECT_TRUE(capsNvlU.numRtStacksPerDssFixedValue);
    EXPECT_FALSE(capsNvlU.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsNvlU.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsNvlU.postImageWriteFlushRequired);
    EXPECT_TRUE(capsNvlU.preImageReadFlushRequired);
    EXPECT_FALSE(capsNvlU.programAdditionalStallPriorToBarrierWithTimestamp);
    EXPECT_FALSE(capsNvlU.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsNvlU.queryPeerAccess);
    EXPECT_TRUE(capsNvlU.rayTracingSupported);
    EXPECT_FALSE(capsNvlU.rayTracingWalkerAdjustmentRequired);
    EXPECT_TRUE(capsNvlU.rcsExposureDisabled);
    EXPECT_FALSE(capsNvlU.reducedSurfaceStateSupported);
    EXPECT_TRUE(capsNvlU.scratchSpaceBasePointerInGrf);
    EXPECT_FALSE(capsNvlU.singleDispatchRequiredForMultiCCS);
    EXPECT_FALSE(capsNvlU.splitMatrixMultiplyAccumulateSupported);
}
