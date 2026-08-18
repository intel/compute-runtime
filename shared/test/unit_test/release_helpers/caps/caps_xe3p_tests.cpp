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

TEST(CapsXe3pTest, givenCriReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsSupported) {
    constexpr auto capsCri = materializeCaps<CapsCri>();
    EXPECT_TRUE(capsCri.isDotProductAccumulateSystolicSupported);
}

TEST(CapsXe3pTest, givenNvlPReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsSupported) {
    constexpr auto capsNvlP = materializeCaps<CapsNvlP>();
    EXPECT_TRUE(capsNvlP.isDotProductAccumulateSystolicSupported);
}
