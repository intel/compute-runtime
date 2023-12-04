/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace MemoryBanks {
constexpr uint32_t bankNotSpecified{0};
constexpr uint32_t mainBank{0};

inline uint32_t getBank(uint32_t deviceOrdinal) {
    return MemoryBanks::mainBank;
}

inline uint32_t getBankForLocalMemory(uint32_t deviceOrdinal) {
    return deviceOrdinal + 1;
}
} // namespace MemoryBanks
