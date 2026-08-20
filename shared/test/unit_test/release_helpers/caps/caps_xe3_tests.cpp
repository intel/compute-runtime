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
    EXPECT_TRUE(capsPtlH.bFloat16ConversionSupported);
    EXPECT_TRUE(capsPtlH.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsPtlH.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsPtlH.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsPtlH.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsPtlH.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3Test, givenPtlUReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsPtlU = materializeCaps<CapsPtlU>();
    EXPECT_FALSE(capsPtlU.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsPtlU.bFloat16ConversionSupported);
    EXPECT_TRUE(capsPtlU.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsPtlU.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsPtlU.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsPtlU.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsPtlU.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3Test, givenWclReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsWcl = materializeCaps<CapsWcl>();
    EXPECT_FALSE(capsWcl.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsWcl.bFloat16ConversionSupported);
    EXPECT_TRUE(capsWcl.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsWcl.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsWcl.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsWcl.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsWcl.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3Test, givenNvlSReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsNvlS = materializeCaps<CapsNvlS>();
    EXPECT_FALSE(capsNvlS.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsNvlS.bFloat16ConversionSupported);
    EXPECT_TRUE(capsNvlS.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsNvlS.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsNvlS.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsNvlS.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsNvlS.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3Test, givenNvlUReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsNvlU = materializeCaps<CapsNvlU>();
    EXPECT_FALSE(capsNvlU.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsNvlU.bFloat16ConversionSupported);
    EXPECT_TRUE(capsNvlU.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsNvlU.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsNvlU.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsNvlU.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsNvlU.splitMatrixMultiplyAccumulateSupported);
}
