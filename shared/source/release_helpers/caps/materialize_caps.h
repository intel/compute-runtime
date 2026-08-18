/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/release_helpers/caps/caps.h"

#include <concepts>
#include <type_traits>

namespace NEO {

#define NEO_CAP_FIELDS(NEO_COPY_CAP_FUNC) \
    NEO_COPY_CAP_FUNC(isDotProductAccumulateSystolicSupported)

template <typename SourceCaps>
constexpr Caps materializeCaps() {
    Caps result{};

#define NEO_COPY_CAP(CAP)                                                               \
    if constexpr (requires { SourceCaps::CAP; }) {                                      \
        using DestinationType = std::remove_cvref_t<decltype(result.CAP)>;              \
        static_assert(std::convertible_to<decltype(SourceCaps::CAP), DestinationType>); \
        result.CAP = SourceCaps::CAP;                                                   \
    }

    NEO_CAP_FIELDS(NEO_COPY_CAP)

#undef NEO_COPY_CAP

    return result;
}

} // namespace NEO
