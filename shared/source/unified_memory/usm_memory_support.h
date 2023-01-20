/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

inline constexpr uint64_t UNIFIED_SHARED_MEMORY_ACCESS = 1 << 0;
inline constexpr uint64_t UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS = 1 << 1;
inline constexpr uint64_t UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS = 1 << 2;
inline constexpr uint64_t UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS = 1 << 3;
