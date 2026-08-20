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
    EXPECT_EQ(materializeCaps<CapsNvlP>(), resolveCapsNvlP(AOT::NVL_P_A0));
    EXPECT_EQ(materializeCaps<CapsNvlP>(), resolveCapsNvlP(AOT::NVL_P_B0));
    EXPECT_EQ(std::nullopt, resolveCapsNvlP(withUnsupportedRevision(AOT::NVL_P_A0)));
}

TEST(CapsXe3pTest, givenCriReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsCri = materializeCaps<CapsCri>();
    EXPECT_FALSE(capsCri.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsCri.bFloat16ConversionSupported);
    EXPECT_TRUE(capsCri.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsCri.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsCri.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsCri.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsCri.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe3pTest, givenNvlPReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsNvlP = materializeCaps<CapsNvlP>();
    EXPECT_FALSE(capsNvlP.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsNvlP.bFloat16ConversionSupported);
    EXPECT_TRUE(capsNvlP.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsNvlP.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsNvlP.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsNvlP.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsNvlP.splitMatrixMultiplyAccumulateSupported);
}
