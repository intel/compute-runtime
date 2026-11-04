/*
 * Copyright (C) 2021-2026 Intel Corporation
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

namespace DotProductInputTypeFlags {
inline constexpr uint32_t int4Pack8 = 1u << 2;
inline constexpr uint32_t int2Pack8 = 1u << 3;
inline constexpr uint32_t fp16Pack2 = 1u << 5;
inline constexpr uint32_t bf16Pack2 = 1u << 6;
inline constexpr uint32_t fp8Pack4 = 1u << 7;
inline constexpr uint32_t bf8Pack4 = 1u << 8;
inline constexpr uint32_t e2m1Pack8 = 1u << 9;
inline constexpr uint32_t int8Pack4 = 1u << 11;
inline constexpr uint32_t scalingUint8 = 1u << 12;
} // namespace DotProductInputTypeFlags

namespace DotProductOutputTypeFlags {
inline constexpr uint32_t int32 = 1u << 0;
inline constexpr uint32_t fp32 = 1u << 1;
inline constexpr uint32_t fp16 = 1u << 2;
inline constexpr uint32_t bf16 = 1u << 3;
} // namespace DotProductOutputTypeFlags
