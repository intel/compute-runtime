/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stdint.h>

constexpr bool isSimd1(uint32_t simdSize) {
    return simdSize == 1u;
}
template <typename WALKER_TYPE>
constexpr typename WALKER_TYPE::SIMD_SIZE getSimdConfig(uint32_t simdSize) {
    return static_cast<typename WALKER_TYPE::SIMD_SIZE>(isSimd1(simdSize) ? (32 >> 4) : (simdSize >> 4));
}
