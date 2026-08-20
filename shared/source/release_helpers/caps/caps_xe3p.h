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

struct CapsXe3pCore {
    static constexpr bool bFloat16ConversionSupported = true;
    static constexpr bool dotProductAccumulateSystolicSupported = true;
};

struct CapsCri : CapsXe3pCore {};
struct CapsNvlP : CapsXe3pCore {};

constexpr std::optional<Caps> resolveCapsCri(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::CRI_A0:
        return materializeCaps<CapsCri>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsNvlP(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::NVL_P_A0:
    case AOT::NVL_P_B0:
        return materializeCaps<CapsNvlP>();
    default:
        return std::nullopt;
    }
}

} // namespace NEO
