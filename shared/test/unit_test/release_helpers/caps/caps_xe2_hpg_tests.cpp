/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/caps/caps_xe2_hpg.h"

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

TEST(CapsXe2HpgTest, givenBmgG21IpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsBmgG21>(), resolveCapsBmgG21(AOT::BMG_G21_A0));
    EXPECT_EQ(materializeCaps<CapsBmgG21>(), resolveCapsBmgG21(AOT::BMG_G21_A1_RESERVED));
    EXPECT_EQ(materializeCaps<CapsBmgG21>(), resolveCapsBmgG21(AOT::BMG_G21_B0_RESERVED));
    EXPECT_EQ(std::nullopt, resolveCapsBmgG21(withUnsupportedRevision(AOT::BMG_G21_A0)));
}

TEST(CapsXe2HpgTest, givenBmgG31IpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsBmgG31>(), resolveCapsBmgG31(AOT::BMG_G31_A0));
    EXPECT_EQ(std::nullopt, resolveCapsBmgG31(withUnsupportedRevision(AOT::BMG_G31_A0)));
}

TEST(CapsXe2HpgTest, givenLnlIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsLnl>(), resolveCapsLnl(AOT::LNL_A0));
    EXPECT_EQ(materializeCaps<CapsLnl>(), resolveCapsLnl(AOT::LNL_A1));
    EXPECT_EQ(materializeCaps<CapsLnl>(), resolveCapsLnl(AOT::LNL_B0));
    EXPECT_EQ(std::nullopt, resolveCapsLnl(withUnsupportedRevision(AOT::LNL_A0)));
}

TEST(CapsXe2HpgTest, givenBmgG21ReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsBmgG21 = materializeCaps<CapsBmgG21>();
    EXPECT_FALSE(capsBmgG21.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsBmgG21.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsBmgG21.bFloat16ConversionSupported);
    EXPECT_TRUE(capsBmgG21.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsBmgG21.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsBmgG21.bindlessAddressingDisabled);
    EXPECT_TRUE(capsBmgG21.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsBmgG21.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsBmgG21.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsBmgG21.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsBmgG21.programAllStateComputeCommandFieldsWARequired);
    EXPECT_TRUE(capsBmgG21.rayTracingSupported);
    EXPECT_TRUE(capsBmgG21.rcsExposureDisabled);
    EXPECT_FALSE(capsBmgG21.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe2HpgTest, givenBmgG31ReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsBmgG31 = materializeCaps<CapsBmgG31>();
    EXPECT_FALSE(capsBmgG31.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsBmgG31.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsBmgG31.bFloat16ConversionSupported);
    EXPECT_TRUE(capsBmgG31.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsBmgG31.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsBmgG31.bindlessAddressingDisabled);
    EXPECT_TRUE(capsBmgG31.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsBmgG31.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsBmgG31.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsBmgG31.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsBmgG31.programAllStateComputeCommandFieldsWARequired);
    EXPECT_TRUE(capsBmgG31.rayTracingSupported);
    EXPECT_TRUE(capsBmgG31.rcsExposureDisabled);
    EXPECT_FALSE(capsBmgG31.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXe2HpgTest, givenLnlReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsLnl = materializeCaps<CapsLnl>();
    EXPECT_FALSE(capsLnl.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsLnl.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsLnl.bFloat16ConversionSupported);
    EXPECT_TRUE(capsLnl.deviceConfigStringTileCountIncluded);
    EXPECT_FALSE(capsLnl.deviceConfigStringXeCuSegmentIncluded);
    EXPECT_FALSE(capsLnl.bindlessAddressingDisabled);
    EXPECT_TRUE(capsLnl.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsLnl.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsLnl.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsLnl.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsLnl.programAllStateComputeCommandFieldsWARequired);
    EXPECT_TRUE(capsLnl.rayTracingSupported);
    EXPECT_TRUE(capsLnl.rcsExposureDisabled);
    EXPECT_FALSE(capsLnl.splitMatrixMultiplyAccumulateSupported);
}
