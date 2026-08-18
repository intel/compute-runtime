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

TEST(CapsGen12LpTest, givenTglReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsNotSupported) {
    constexpr auto capsTgl = materializeCaps<CapsTgl>();
    EXPECT_FALSE(capsTgl.isDotProductAccumulateSystolicSupported);
}

TEST(CapsGen12LpTest, givenRklReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsNotSupported) {
    constexpr auto capsRkl = materializeCaps<CapsRkl>();
    EXPECT_FALSE(capsRkl.isDotProductAccumulateSystolicSupported);
}

TEST(CapsGen12LpTest, givenAdlSReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsNotSupported) {
    constexpr auto capsAdlS = materializeCaps<CapsAdlS>();
    EXPECT_FALSE(capsAdlS.isDotProductAccumulateSystolicSupported);
}

TEST(CapsGen12LpTest, givenAdlPReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsNotSupported) {
    constexpr auto capsAdlP = materializeCaps<CapsAdlP>();
    EXPECT_FALSE(capsAdlP.isDotProductAccumulateSystolicSupported);
}

TEST(CapsGen12LpTest, givenAdlNReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsNotSupported) {
    constexpr auto capsAdlN = materializeCaps<CapsAdlN>();
    EXPECT_FALSE(capsAdlN.isDotProductAccumulateSystolicSupported);
}

TEST(CapsGen12LpTest, givenDg1ReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsNotSupported) {
    constexpr auto capsDg1 = materializeCaps<CapsDg1>();
    EXPECT_FALSE(capsDg1.isDotProductAccumulateSystolicSupported);
}
