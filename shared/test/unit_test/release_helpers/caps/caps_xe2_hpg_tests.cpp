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

TEST(CapsXe2HpgTest, givenBmgG21ReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsSupported) {
    constexpr auto capsBmgG21 = materializeCaps<CapsBmgG21>();
    EXPECT_TRUE(capsBmgG21.isDotProductAccumulateSystolicSupported);
}

TEST(CapsXe2HpgTest, givenBmgG31ReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsSupported) {
    constexpr auto capsBmgG31 = materializeCaps<CapsBmgG31>();
    EXPECT_TRUE(capsBmgG31.isDotProductAccumulateSystolicSupported);
}

TEST(CapsXe2HpgTest, givenLnlReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsSupported) {
    constexpr auto capsLnl = materializeCaps<CapsLnl>();
    EXPECT_TRUE(capsLnl.isDotProductAccumulateSystolicSupported);
}
