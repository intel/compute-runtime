/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace MemoryBanks {
constexpr uint32_t BankNotSpecified{0};
constexpr uint32_t MainBank{0};

inline uint32_t getBank(uint32_t deviceOrdinal) {
    return MemoryBanks::MainBank;
}
} // namespace MemoryBanks
