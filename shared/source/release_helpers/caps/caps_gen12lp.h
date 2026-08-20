/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/release_helpers/caps/materialize_caps.h"

#include <optional>

namespace NEO {

struct CapsGen12Lp {
    static constexpr bool bFloat16ConversionSupported = true;
};

struct CapsTgl : CapsGen12Lp {};
struct CapsRkl : CapsGen12Lp {};
struct CapsAdlS : CapsGen12Lp {};
struct CapsAdlP : CapsGen12Lp {};
struct CapsAdlN : CapsGen12Lp {};
struct CapsDg1 : CapsGen12Lp {};

constexpr std::optional<Caps> resolveCapsTgl(HardwareIpVersion) {
    return materializeCaps<CapsTgl>();
}
constexpr std::optional<Caps> resolveCapsRkl(HardwareIpVersion) {
    return materializeCaps<CapsRkl>();
}
constexpr std::optional<Caps> resolveCapsAdlS(HardwareIpVersion) {
    return materializeCaps<CapsAdlS>();
}
constexpr std::optional<Caps> resolveCapsAdlP(HardwareIpVersion) {
    return materializeCaps<CapsAdlP>();
}
constexpr std::optional<Caps> resolveCapsAdlN(HardwareIpVersion) {
    return materializeCaps<CapsAdlN>();
}
constexpr std::optional<Caps> resolveCapsDg1(HardwareIpVersion) {
    return materializeCaps<CapsDg1>();
}

} // namespace NEO
