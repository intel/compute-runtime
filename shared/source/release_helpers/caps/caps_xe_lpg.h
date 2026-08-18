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

struct CapsXeLpgCore {
    static constexpr bool isDotProductAccumulateSystolicSupported = false;
};

struct CapsMtlU : CapsXeLpgCore {};
struct CapsMtlH : CapsXeLpgCore {};
struct CapsArlH : CapsXeLpgCore {
    static constexpr bool isDotProductAccumulateSystolicSupported = true;
};

constexpr std::optional<Caps> resolveCapsMtlU(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::MTL_U_A0:
    case AOT::MTL_U_B0:
        return materializeCaps<CapsMtlU>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsMtlH(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::MTL_H_A0:
    case AOT::MTL_H_B0:
        return materializeCaps<CapsMtlH>();
    default:
        return std::nullopt;
    }
}

constexpr std::optional<Caps> resolveCapsArlH(HardwareIpVersion ipVersion) {
    switch (ipVersion.value) {
    case AOT::ARL_H_A0:
    case AOT::ARL_H_B0:
        return materializeCaps<CapsArlH>();
    default:
        return std::nullopt;
    }
}

} // namespace NEO
