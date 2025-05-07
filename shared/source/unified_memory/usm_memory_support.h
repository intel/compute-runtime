/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>

namespace UnifiedSharedMemoryFlags {

inline constexpr uint64_t access = 1 << 0;
inline constexpr uint64_t atomicAccess = 1 << 1;
inline constexpr uint64_t concurrentAccess = 1 << 2;
inline constexpr uint64_t concurrentAtomicAccess = 1 << 3;
} // namespace UnifiedSharedMemoryFlags
