/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/release_helpers/caps/materialize_caps.h"

#include "neo_aot_platforms.h"

#include <optional>

namespace NEO {

struct CapsXe2HpgCore {
    static constexpr bool bFloat16ConversionSupported = true;
    static constexpr bool dotProductAccumulateSystolicSupported = true;
};

struct CapsBmgG21 : CapsXe2HpgCore {};
struct CapsBmgG31 : CapsXe2HpgCore {};
struct CapsLnl : CapsXe2HpgCore {};

constexpr std::optional<Caps> resolveCapsBmgG21(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::BMG_G21_A0:
    case AOT::BMG_G21_A1_RESERVED:
    case AOT::BMG_G21_B0_RESERVED:
        return materializeCaps<CapsBmgG21>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsBmgG31(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::BMG_G31_A0:
        return materializeCaps<CapsBmgG31>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsLnl(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::LNL_A0:
    case AOT::LNL_A1:
    case AOT::LNL_B0:
        return materializeCaps<CapsLnl>();
    default:
        return std::nullopt;
    }
}

} // namespace NEO
