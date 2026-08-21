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
    EXPECT_FALSE(capsTgl.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsTgl.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsTgl.bFloat16ConversionSupported);
    EXPECT_TRUE(capsTgl.bindlessAddressingDisabled);
    EXPECT_FALSE(capsTgl.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsTgl.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsTgl.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsTgl.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsTgl.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsTgl.rcsExposureDisabled);
    EXPECT_FALSE(capsTgl.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsGen12LpTest, givenRklReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsRkl = materializeCaps<CapsRkl>();
    EXPECT_FALSE(capsRkl.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsRkl.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsRkl.bFloat16ConversionSupported);
    EXPECT_TRUE(capsRkl.bindlessAddressingDisabled);
    EXPECT_FALSE(capsRkl.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsRkl.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsRkl.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsRkl.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsRkl.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsRkl.rcsExposureDisabled);
    EXPECT_FALSE(capsRkl.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsGen12LpTest, givenAdlSReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsAdlS = materializeCaps<CapsAdlS>();
    EXPECT_FALSE(capsAdlS.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsAdlS.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsAdlS.bFloat16ConversionSupported);
    EXPECT_TRUE(capsAdlS.bindlessAddressingDisabled);
    EXPECT_FALSE(capsAdlS.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsAdlS.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsAdlS.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsAdlS.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsAdlS.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsAdlS.rcsExposureDisabled);
    EXPECT_FALSE(capsAdlS.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsGen12LpTest, givenAdlPReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsAdlP = materializeCaps<CapsAdlP>();
    EXPECT_FALSE(capsAdlP.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsAdlP.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsAdlP.bFloat16ConversionSupported);
    EXPECT_TRUE(capsAdlP.bindlessAddressingDisabled);
    EXPECT_FALSE(capsAdlP.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsAdlP.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsAdlP.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsAdlP.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsAdlP.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsAdlP.rcsExposureDisabled);
    EXPECT_FALSE(capsAdlP.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsGen12LpTest, givenAdlNReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsAdlN = materializeCaps<CapsAdlN>();
    EXPECT_FALSE(capsAdlN.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsAdlN.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsAdlN.bFloat16ConversionSupported);
    EXPECT_TRUE(capsAdlN.bindlessAddressingDisabled);
    EXPECT_FALSE(capsAdlN.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsAdlN.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsAdlN.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsAdlN.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsAdlN.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsAdlN.rcsExposureDisabled);
    EXPECT_FALSE(capsAdlN.splitMatrixMultiplyAccumulateSupported);
}

TEST(CapsGen12LpTest, givenDg1ReleaseWhenMaterializingCapsThenCapabilitiesAreCorrect) {
    constexpr auto capsDg1 = materializeCaps<CapsDg1>();
    EXPECT_FALSE(capsDg1.adjustWalkOrderAvailable);
    EXPECT_FALSE(capsDg1.auxSurfaceModeOverrideRequired);
    EXPECT_TRUE(capsDg1.bFloat16ConversionSupported);
    EXPECT_TRUE(capsDg1.bindlessAddressingDisabled);
    EXPECT_FALSE(capsDg1.dotProductAccumulateSystolicSupported);
    EXPECT_FALSE(capsDg1.globalBindlessAllocatorEnabled);
    EXPECT_FALSE(capsDg1.pipeControlPriorToNonPipelinedStateCommandsBaseWARequired);
    EXPECT_FALSE(capsDg1.pipeControlPriorToPipelineSelectWaRequired);
    EXPECT_FALSE(capsDg1.programAllStateComputeCommandFieldsWARequired);
    EXPECT_FALSE(capsDg1.rcsExposureDisabled);
    EXPECT_FALSE(capsDg1.splitMatrixMultiplyAccumulateSupported);
}
