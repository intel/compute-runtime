/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <type_traits>

namespace NEO {
template <typename To, typename From>
constexpr To safePodCast(From *f) {
    typedef typename std::remove_pointer<From>::type FromType;
    typedef typename std::remove_pointer<To>::type ToType;

    static_assert(std::is_trivially_copyable<FromType>::value &&
                      std::is_standard_layout<FromType>::value,
                  "Original cast type is not POD");

    static_assert(std::is_trivially_copyable<ToType>::value &&
                      std::is_standard_layout<ToType>::value,
                  "Target cast type is not POD");

    return reinterpret_cast<To>(f);
}
} // namespace NEO
