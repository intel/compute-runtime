/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stdint.h>

constexpr bool isSimd1(uint32_t simdSize) {
    return simdSize == 1u;
}
template <typename DefaultWalkerType>
constexpr typename DefaultWalkerType::SIMD_SIZE getSimdConfig(uint32_t simdSize) {
    return static_cast<typename DefaultWalkerType::SIMD_SIZE>(isSimd1(simdSize) ? (32 >> 4) : (simdSize >> 4)); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
}
