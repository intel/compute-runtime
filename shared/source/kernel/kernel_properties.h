/*
 * Copyright (C) 2021-2023 Intel Corporation
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
} // namespace FpAtomicExtFlags
