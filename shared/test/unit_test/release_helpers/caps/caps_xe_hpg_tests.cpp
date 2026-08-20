/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/caps/caps_xe_hpg.h"

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

TEST(CapsXeHpgTest, givenDg2G10IpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsDg2G10>(), resolveCapsDg2G10(AOT::DG2_G10_A0));
    EXPECT_EQ(materializeCaps<CapsDg2G10>(), resolveCapsDg2G10(AOT::DG2_G10_A1));
    EXPECT_EQ(materializeCaps<CapsDg2G10>(), resolveCapsDg2G10(AOT::DG2_G10_B0));
    EXPECT_EQ(materializeCaps<CapsDg2G10>(), resolveCapsDg2G10(AOT::DG2_G10_C0));
    EXPECT_EQ(std::nullopt, resolveCapsDg2G10(withUnsupportedRevision(AOT::DG2_G10_A0)));
}

TEST(CapsXeHpgTest, givenDg2G11IpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsDg2G11>(), resolveCapsDg2G11(AOT::DG2_G11_A0));
    EXPECT_EQ(materializeCaps<CapsDg2G11>(), resolveCapsDg2G11(AOT::DG2_G11_B0));
    EXPECT_EQ(materializeCaps<CapsDg2G11>(), resolveCapsDg2G11(AOT::DG2_G11_B1));
    EXPECT_EQ(std::nullopt, resolveCapsDg2G11(withUnsupportedRevision(AOT::DG2_G11_A0)));
}

TEST(CapsXeHpgTest, givenDg2G12IpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsDg2G12>(), resolveCapsDg2G12(AOT::DG2_G12_A0));
    EXPECT_EQ(std::nullopt, resolveCapsDg2G12(withUnsupportedRevision(AOT::DG2_G12_A0)));
}

TEST(CapsXeHpgTest, givenDg2G10ReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsDg2G10 = materializeCaps<CapsDg2G10>();
    EXPECT_FALSE(capsDg2G10.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsDg2G10.bFloat16ConversionSupported);
    EXPECT_TRUE(capsDg2G10.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsDg2G10.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsDg2G10.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_TRUE(capsDg2G10.programAllStateComputeCommandFieldsWARequired);
    EXPECT_TRUE(capsDg2G10.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXeHpgTest, givenDg2G11ReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsDg2G11 = materializeCaps<CapsDg2G11>();
    EXPECT_FALSE(capsDg2G11.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsDg2G11.bFloat16ConversionSupported);
    EXPECT_TRUE(capsDg2G11.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsDg2G11.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsDg2G11.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_TRUE(capsDg2G11.programAllStateComputeCommandFieldsWARequired);
    EXPECT_TRUE(capsDg2G11.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXeHpgTest, givenDg2G12ReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsDg2G12 = materializeCaps<CapsDg2G12>();
    EXPECT_FALSE(capsDg2G12.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsDg2G12.bFloat16ConversionSupported);
    EXPECT_TRUE(capsDg2G12.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsDg2G12.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsDg2G12.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_TRUE(capsDg2G12.programAllStateComputeCommandFieldsWARequired);
    EXPECT_TRUE(capsDg2G12.splitMatrixMultiplyAccumulateSupported);
}
