/*
 * Copyright (C) 2018-2021 Intel Corporation
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

inline uint32_t getBankForLocalMemory(uint32_t deviceOrdinal) {
    return deviceOrdinal + 1;
}
} // namespace MemoryBanks
