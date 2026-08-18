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

struct CapsXe3Core {
    static constexpr bool isDotProductAccumulateSystolicSupported = true;
};

struct CapsPtlH : CapsXe3Core {};
struct CapsPtlU : CapsXe3Core {};
struct CapsWcl : CapsXe3Core {};
struct CapsNvlS : CapsXe3Core {};
struct CapsNvlU : CapsXe3Core {};

constexpr std::optional<Caps> resolveCapsPtlH(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::PTL_H_A0:
    case AOT::PTL_H_B0:
        return materializeCaps<CapsPtlH>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsPtlU(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::PTL_U_A0:
        return materializeCaps<CapsPtlU>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsWcl(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::WCL_A0:
    case AOT::WCL_A1:
        return materializeCaps<CapsWcl>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsNvlS(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::NVL_S_A0:
    case AOT::NVL_S_B0:
        return materializeCaps<CapsNvlS>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsNvlU(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::NVL_U_A0:
    case AOT::NVL_U_A1:
    case AOT::NVL_U_B0:
        return materializeCaps<CapsNvlU>();
    default:
        return std::nullopt;
    }
}

} // namespace NEO
