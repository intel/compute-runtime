/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <stdint.h>

template <typename WALKER_TYPE>
constexpr typename WALKER_TYPE::SIMD_SIZE getSimdConfig(uint32_t simdSize) {
    return static_cast<typename WALKER_TYPE::SIMD_SIZE>((simdSize == 1) ? (32 >> 4) : (simdSize >> 4));
}
