/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <type_traits>

namespace NEO {
namespace TypeTraits {

template <typename T>
constexpr bool isPodV = std::is_standard_layout_v<T> && std::is_trivial_v<T> && std::is_trivially_copyable_v<T>;

} // namespace TypeTraits
} // namespace NEO
