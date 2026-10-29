/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {

template <typename GfxFamily>
struct SBAPlaceholder {};

template <typename GfxFamily>
concept GfxFamilyWithSBA = requires() {
    typename GfxFamily::STATE_BASE_ADDRESS;
};

template <typename GfxFamily>
inline bool isStateBaseAddressProgrammingEnabled() {
    if constexpr (requires { GfxFamily::isStateBaseAddressProgrammingEnabled(); }) {
        return GfxFamily::isStateBaseAddressProgrammingEnabled();
    }
    return true;
}

template <typename GfxFamily>
struct StateBaseAddressTypeHelper;

template <typename GfxFamily>
struct StateBaseAddressTypeHelper {
    using type = SBAPlaceholder<GfxFamily>;
};

template <GfxFamilyWithSBA Family>
struct StateBaseAddressTypeHelper<Family> {
    using type = typename Family::STATE_BASE_ADDRESS;
};

} // namespace NEO
