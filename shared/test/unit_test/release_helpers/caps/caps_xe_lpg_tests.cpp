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
    EXPECT_TRUE(capsMtlUA0.bFloat16ConversionSupported);
    EXPECT_FALSE(capsMtlUA0.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsMtlUA0.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsMtlUA0.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_TRUE(capsMtlUA0.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsMtlUA0.splitMatrixMultiplyAccumulateSupported);

    EXPECT_FALSE(capsMtlUB0.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsMtlUB0.bFloat16ConversionSupported);
    EXPECT_FALSE(capsMtlUB0.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsMtlUB0.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsMtlUB0.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsMtlUB0.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsMtlUB0.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXeLpgTest, givenMtlHReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsMtlHA0 = materializeCaps<CapsMtlHA0>();
    constexpr auto capsMtlHB0 = materializeCaps<CapsMtlHB0>();

    EXPECT_FALSE(capsMtlHA0.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsMtlHA0.bFloat16ConversionSupported);
    EXPECT_FALSE(capsMtlHA0.dotProductAccumulateSystolicSupported);
    EXPECT_TRUE(capsMtlHA0.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsMtlHA0.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_TRUE(capsMtlHA0.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsMtlHA0.splitMatrixMultiplyAccumulateSupported);

    EXPECT_FALSE(capsMtlHB0.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsMtlHB0.bFloat16ConversionSupported);
    EXPECT_FALSE(capsMtlHB0.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsMtlHB0.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsMtlHB0.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsMtlHB0.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsMtlHB0.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsXeLpgTest, givenArlHReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsArlH = materializeCaps<CapsArlH>();

    EXPECT_TRUE(capsArlH.adjustWalkOrderAvailable);
    EXPECT_TRUE(capsArlH.bFloat16ConversionSupported);
    EXPECT_TRUE(capsArlH.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsArlH.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_TRUE(capsArlH.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsArlH.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsArlH.splitMatrixMultiplyAccumulateSupported);
}
