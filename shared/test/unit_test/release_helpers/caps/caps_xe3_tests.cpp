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

TEST(CapsXe3Test, givenPtlHReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsSupported) {
    constexpr auto capsPtlH = materializeCaps<CapsPtlH>();
    EXPECT_TRUE(capsPtlH.isDotProductAccumulateSystolicSupported);
}

TEST(CapsXe3Test, givenPtlUReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsSupported) {
    constexpr auto capsPtlU = materializeCaps<CapsPtlU>();
    EXPECT_TRUE(capsPtlU.isDotProductAccumulateSystolicSupported);
}

TEST(CapsXe3Test, givenWclReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsSupported) {
    constexpr auto capsWcl = materializeCaps<CapsWcl>();
    EXPECT_TRUE(capsWcl.isDotProductAccumulateSystolicSupported);
}

TEST(CapsXe3Test, givenNvlSReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsSupported) {
    constexpr auto capsNvlS = materializeCaps<CapsNvlS>();
    EXPECT_TRUE(capsNvlS.isDotProductAccumulateSystolicSupported);
}

TEST(CapsXe3Test, givenNvlUReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsSupported) {
    constexpr auto capsNvlU = materializeCaps<CapsNvlU>();
    EXPECT_TRUE(capsNvlU.isDotProductAccumulateSystolicSupported);
}
