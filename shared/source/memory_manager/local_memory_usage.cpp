/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/local_memory_usage.h"

#include <algorithm>
#include <bitset>
#include <iterator>

namespace NEO {

LocalMemoryUsageBankSelector::LocalMemoryUsageBankSelector(uint32_t banksCount) : banksCount(banksCount) {
    UNRECOVERABLE_IF(banksCount == 0);

    memorySizes.reset(new std::atomic<uint64_t>[banksCount]);
    for (uint32_t i = 0; i < banksCount; i++) {
        memorySizes[i] = 0;
    }
}

uint32_t LocalMemoryUsageBankSelector::getLeastOccupiedBank() {
    auto leastOccupiedBankIterator = std::min_element(memorySizes.get(), memorySizes.get() + banksCount);
    return static_cast<uint32_t>(std::distance(memorySizes.get(), leastOccupiedBankIterator));
}

void LocalMemoryUsageBankSelector::freeOnBank(uint32_t bankIndex, uint64_t allocationSize) {
    UNRECOVERABLE_IF(bankIndex >= banksCount);
    memorySizes[bankIndex] -= allocationSize;
}
void LocalMemoryUsageBankSelector::reserveOnBank(uint32_t bankIndex, uint64_t allocationSize) {
    UNRECOVERABLE_IF(bankIndex >= banksCount);
    memorySizes[bankIndex] += allocationSize;
}

void LocalMemoryUsageBankSelector::updateUsageInfo(uint32_t memoryBanks, uint64_t allocationSize, bool reserve) {
    auto banks = std::bitset<32>(memoryBanks);
    for (uint32_t bankIndex = 0; bankIndex < banks.size() && bankIndex < banksCount; bankIndex++) {
        if (banks.test(bankIndex)) {
            if (reserve) {
                reserveOnBank(bankIndex, allocationSize);
            } else {
                freeOnBank(bankIndex, allocationSize);
            }
        }
    }
}

} // namespace NEO
