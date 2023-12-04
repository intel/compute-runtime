/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stddef.h>

template <typename T, size_t n>
constexpr size_t arrayCount(const T (&)[n]) {
    return n;
}

template <typename T, size_t n>
constexpr bool isInRange(size_t idx, const T (&)[n]) {
    return (idx < n);
}
