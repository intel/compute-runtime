/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <stddef.h>

template <typename T, size_t N>
constexpr size_t arrayCount(const T (&)[N]) {
    return N;
}

template <typename T, size_t N>
constexpr bool isInRange(size_t idx, const T (&)[N]) {
    return (idx < N);
}
