/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <type_traits>

namespace NEO {

// std::to_underlying is C++23 feature
template <typename EnumT>
constexpr auto toUnderlying(EnumT scopedEnumValue) {
    static_assert(std::is_enum_v<EnumT>);
    static_assert(!std::is_convertible_v<EnumT, std::underlying_type_t<EnumT>>);

    return static_cast<std::underlying_type_t<EnumT>>(scopedEnumValue);
}

template <typename EnumT>
    requires(std::is_enum_v<EnumT> && !std::is_convertible_v<EnumT, std::underlying_type_t<EnumT>>)
constexpr auto toEnum(std::underlying_type_t<EnumT> region) {
    return static_cast<EnumT>(region);
}

} // namespace NEO
