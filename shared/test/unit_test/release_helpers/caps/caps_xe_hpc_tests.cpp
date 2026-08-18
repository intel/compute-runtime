/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/caps/caps_xe_hpc.h"

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

TEST(CapsXeHpcTest, givenPvcIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsPvc>(), resolveCapsPvc(AOT::PVC_XL_A0));
    EXPECT_EQ(materializeCaps<CapsPvc>(), resolveCapsPvc(AOT::PVC_XL_A0P));
    EXPECT_EQ(materializeCaps<CapsPvc>(), resolveCapsPvc(AOT::PVC_XT_A0));
    EXPECT_EQ(materializeCaps<CapsPvc>(), resolveCapsPvc(AOT::PVC_XT_B0));
    EXPECT_EQ(materializeCaps<CapsPvc>(), resolveCapsPvc(AOT::PVC_XT_B1));
    EXPECT_EQ(materializeCaps<CapsPvc>(), resolveCapsPvc(AOT::PVC_XT_C0));
    EXPECT_EQ(std::nullopt, resolveCapsPvc(withUnsupportedRevision(AOT::PVC_XL_A0)));
}

TEST(CapsXeHpcTest, givenPvcVgIpVersionWhenResolvingCapsThenReleaseCapsAreReturned) {
    EXPECT_EQ(materializeCaps<CapsPvcVg>(), resolveCapsPvcVg(AOT::PVC_XT_C0_VG));
    EXPECT_EQ(std::nullopt, resolveCapsPvcVg(withUnsupportedRevision(AOT::PVC_XT_C0_VG)));
}

TEST(CapsXeHpcTest, givenPvcReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsSupported) {
    constexpr auto capsPvc = materializeCaps<CapsPvc>();
    EXPECT_TRUE(capsPvc.isDotProductAccumulateSystolicSupported);
}

TEST(CapsXeHpcTest, givenPvcVgReleaseWhenMaterializingCapsThenDotProductAccumulateSystolicIsNotSupportedByAnyStepping) {
    constexpr auto capsPvcVg = materializeCaps<CapsPvcVg>();
    EXPECT_FALSE(capsPvcVg.isDotProductAccumulateSystolicSupported);
}
