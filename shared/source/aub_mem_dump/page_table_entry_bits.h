/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <limits>

namespace PageTableEntry {
inline constexpr uint32_t presentBit = 0;
inline constexpr uint32_t writableBit = 1;
inline constexpr uint32_t userSupervisorBit = 2;
inline constexpr uint32_t localMemoryBit = 11;
inline constexpr uint64_t nonValidBits = std::numeric_limits<uint64_t>::max();
} // namespace PageTableEntry
