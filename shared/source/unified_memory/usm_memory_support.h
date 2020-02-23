/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <cstdint>

constexpr uint64_t UNIFIED_SHARED_MEMORY_ACCESS = 1 << 0;
constexpr uint64_t UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS = 1 << 1;
constexpr uint64_t UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS = 1 << 2;
constexpr uint64_t UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS = 1 << 3;
