/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helpers/caps/caps_setup.h"

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helpers/caps/apply_macro_for_all_releases.h"

#include "caps_all.h"

#include <cstdint>

namespace NEO {

namespace {
constexpr uint32_t getCapsKey(HardwareIpVersion hwIpVersion) {
    return static_cast<uint32_t>(hwIpVersion.architecture) * 100u + hwIpVersion.release;
}
} // namespace

#define NEO_CAPS_CASE(NAME, PRODUCT) \
    case ReleaseType::NAME:          \
        return resolveCaps##PRODUCT(ipVersion);

std::optional<Caps> resolveCaps(const HardwareIpVersion &ipVersion) {
    switch (static_cast<ReleaseType>(getCapsKey(ipVersion))) {
        NEO_APPLY_MACRO_FOR_ALL_RELEASES(NEO_CAPS_CASE)
    default:
        return std::nullopt;
    }
}

void setupCaps(HardwareInfo &hwInfo) {
    auto caps = resolveCaps(hwInfo.ipVersion);
    UNRECOVERABLE_IF(!caps.has_value());
    hwInfo.caps = caps.value();
}

} // namespace NEO
