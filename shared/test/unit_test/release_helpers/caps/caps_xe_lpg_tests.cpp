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
    EXPECT_EQ(materializeCaps<CapsMtlU>(), resolveCapsMtlU(AOT::MTL_U_A0));
    EXPECT_EQ(materializeCaps<CapsMtlU>(), resolveCapsMtlU(AOT::MTL_U_B0));
    EXPECT_EQ(std::nullopt, resolveCapsMtlU(withUnsupportedRevision(AOT::MTL_U_A0)));
}

TEST(CapsXeLpgTest, givenMtlHIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsMtlH>(), resolveCapsMtlH(AOT::MTL_H_A0));
    EXPECT_EQ(materializeCaps<CapsMtlH>(), resolveCapsMtlH(AOT::MTL_H_B0));
    EXPECT_EQ(std::nullopt, resolveCapsMtlH(withUnsupportedRevision(AOT::MTL_H_A0)));
}

TEST(CapsXeLpgTest, givenArlHIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsArlH>(), resolveCapsArlH(AOT::ARL_H_A0));
    EXPECT_EQ(materializeCaps<CapsArlH>(), resolveCapsArlH(AOT::ARL_H_B0));
    EXPECT_EQ(std::nullopt, resolveCapsArlH(withUnsupportedRevision(AOT::ARL_H_A0)));
}

TEST(CapsXeLpgTest, givenMtlUReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsNotSupportedByAnyStepping) {
    constexpr auto capsMtlU = materializeCaps<CapsMtlU>();
    EXPECT_FALSE(capsMtlU.isDotProductAccumulateSystolicSupported);
}

TEST(CapsXeLpgTest, givenMtlHReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsNotSupportedByAnyStepping) {
    constexpr auto capsMtlH = materializeCaps<CapsMtlH>();
    EXPECT_FALSE(capsMtlH.isDotProductAccumulateSystolicSupported);
}

TEST(CapsXeLpgTest, givenArlHReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsSupported) {
    constexpr auto capsArlH = materializeCaps<CapsArlH>();
    EXPECT_TRUE(capsArlH.isDotProductAccumulateSystolicSupported);
}
