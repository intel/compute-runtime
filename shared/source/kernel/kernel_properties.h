/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
namespace FpAtomicExtFlags {
inline constexpr uint64_t globalLoadStore = 1 << 0; // Supports atomic load, store, and exchange
inline constexpr uint64_t globalAdd = 1 << 1;       // Supports atomic add and subtract
inline constexpr uint64_t globalMinMax = 1 << 2;    // Supports atomic min and max
inline constexpr uint64_t localLoadStore = 1 << 16; // Supports atomic load, store, and exchange
inline constexpr uint64_t localAdd = 1 << 17;       // Supports atomic add and subtract
inline constexpr uint64_t localMinMax = 1 << 18;    // Supports atomic min and max

inline constexpr uint32_t loadStoreAtomicCaps = (0u | FpAtomicExtFlags::globalLoadStore | FpAtomicExtFlags::localLoadStore);
inline constexpr uint32_t minMaxAtomicCaps = (0u | FpAtomicExtFlags::globalMinMax | FpAtomicExtFlags::localMinMax);
inline constexpr uint32_t addAtomicCaps = (0u | FpAtomicExtFlags::globalAdd | FpAtomicExtFlags::localAdd);
} // namespace FpAtomicExtFlags
