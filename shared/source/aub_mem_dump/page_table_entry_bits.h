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
constexpr uint32_t presentBit = 0;
constexpr uint32_t writableBit = 1;
constexpr uint32_t userSupervisorBit = 2;
constexpr uint32_t localMemoryBit = 11;
constexpr uint64_t nonValidBits = std::numeric_limits<uint64_t>::max();
} // namespace PageTableEntry
