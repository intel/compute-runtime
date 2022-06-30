/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <limits>

namespace PageTableEntry {
const uint32_t presentBit = 0;
const uint32_t writableBit = 1;
const uint32_t userSupervisorBit = 2;
const uint32_t localMemoryBit = 11;
const uint64_t nonValidBits = std::numeric_limits<uint64_t>::max();
} // namespace PageTableEntry
