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
    EXPECT_EQ(materializeCaps<CapsPtlH>(), resolveCapsPtlH(AOT::PTL_H_A0));
    EXPECT_EQ(materializeCaps<CapsPtlH>(), resolveCapsPtlH(AOT::PTL_H_B0));
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
    constexpr auto capsPtlH = materializeCaps<CapsPtlH>();
    EXPECT_FALSE(capsPtlH.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsPtlH.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsPtlH.bFloat16ConversionSupported);
    EXPECT_TRUE(capsPtlH.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsPtlH.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsPtlH.bindlessAddressingDisabled);
    EXPECT_TRUE(capsPtlH.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsPtlH.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsPtlH.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsPtlH.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsPtlH.programAllStateComputeCommandFieldsWARequired);
    EXPECT_TRUE(capsPtlH.rayTracingSupported);
    EXPECT_TRUE(capsPtlH.rcsExposureDisabled);
    EXPECT_FALSE(capsPtlH.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3Test, givenPtlUReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsPtlU = materializeCaps<CapsPtlU>();
    EXPECT_FALSE(capsPtlU.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsPtlU.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsPtlU.bFloat16ConversionSupported);
    EXPECT_TRUE(capsPtlU.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsPtlU.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsPtlU.bindlessAddressingDisabled);
    EXPECT_TRUE(capsPtlU.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsPtlU.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsPtlU.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsPtlU.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsPtlU.programAllStateComputeCommandFieldsWARequired);
    EXPECT_TRUE(capsPtlU.rayTracingSupported);
    EXPECT_TRUE(capsPtlU.rcsExposureDisabled);
    EXPECT_FALSE(capsPtlU.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3Test, givenWclReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsWcl = materializeCaps<CapsWcl>();
    EXPECT_FALSE(capsWcl.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsWcl.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsWcl.bFloat16ConversionSupported);
    EXPECT_TRUE(capsWcl.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsWcl.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsWcl.bindlessAddressingDisabled);
    EXPECT_TRUE(capsWcl.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsWcl.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsWcl.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsWcl.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsWcl.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsWcl.rayTracingSupported);
    EXPECT_TRUE(capsWcl.rcsExposureDisabled);
    EXPECT_FALSE(capsWcl.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3Test, givenNvlSReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsNvlS = materializeCaps<CapsNvlS>();
    EXPECT_FALSE(capsNvlS.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsNvlS.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsNvlS.bFloat16ConversionSupported);
    EXPECT_TRUE(capsNvlS.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsNvlS.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsNvlS.bindlessAddressingDisabled);
    EXPECT_TRUE(capsNvlS.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsNvlS.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsNvlS.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsNvlS.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsNvlS.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsNvlS.rayTracingSupported);
    EXPECT_TRUE(capsNvlS.rcsExposureDisabled);
    EXPECT_FALSE(capsNvlS.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3Test, givenNvlUReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsNvlU = materializeCaps<CapsNvlU>();
    EXPECT_FALSE(capsNvlU.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsNvlU.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsNvlU.bFloat16ConversionSupported);
    EXPECT_TRUE(capsNvlU.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsNvlU.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsNvlU.bindlessAddressingDisabled);
    EXPECT_TRUE(capsNvlU.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsNvlU.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsNvlU.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsNvlU.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsNvlU.programAllStateComputeCommandFieldsWARequired);
    EXPECT_TRUE(capsNvlU.rayTracingSupported);
    EXPECT_TRUE(capsNvlU.rcsExposureDisabled);
    EXPECT_FALSE(capsNvlU.splitMatrixMultiplyAccumulateSupported);
}
