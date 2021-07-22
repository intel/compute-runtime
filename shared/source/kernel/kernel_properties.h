/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

constexpr uint64_t FP_ATOMIC_EXT_FLAG_GLOBAL_LOAD_STORE = 1 << 0; // Supports atomic load, store, and exchange
constexpr uint64_t FP_ATOMIC_EXT_FLAG_GLOBAL_ADD = 1 << 1;        // Supports atomic add and subtract
constexpr uint64_t FP_ATOMIC_EXT_FLAG_GLOBAL_MIN_MAX = 1 << 2;    // Supports atomic min and max
constexpr uint64_t FP_ATOMIC_EXT_FLAG_LOCAL_LOAD_STORE = 1 << 16; // Supports atomic load, store, and exchange
constexpr uint64_t FP_ATOMIC_EXT_FLAG_LOCAL_ADD = 1 << 17;        // Supports atomic add and subtract
constexpr uint64_t FP_ATOMIC_EXT_FLAG_LOCAL_MIN_MAX = 1 << 18;    // Supports atomic min and max
