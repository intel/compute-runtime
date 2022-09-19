/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/local_memory_usage.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/common_types.h"

#include <algorithm>
#include <bitset>
#include <iterator>
#include <limits>

namespace NEO {

LocalMemoryUsageBankSelector::LocalMemoryUsageBankSelector(uint32_t banksCount) : banksCount(banksCount) {
    UNRECOVERABLE_IF(banksCount == 0);

    memorySizes.reset(new std::atomic<uint64_t>[banksCount]);
    for (uint32_t i = 0; i < banksCount; i++) {
        memorySizes[i] = 0;
    }
}

uint32_t LocalMemoryUsageBankSelector::getLeastOccupiedBank(DeviceBitfield deviceBitfield) {
    if (DebugManager.flags.OverrideLeastOccupiedBank.get() != -1) {
        return static_cast<uint32_t>(DebugManager.flags.OverrideLeastOccupiedBank.get());
    }
    uint32_t leastOccupiedBank = 0u;
    uint64_t smallestViableMemorySize = std::numeric_limits<uint64_t>::max();

    UNRECOVERABLE_IF(deviceBitfield.count() == 0);
    for (uint32_t i = 0u; i < banksCount; i++) {
        if (deviceBitfield.test(i)) {
            if (memorySizes[i] < smallestViableMemorySize) {
                leastOccupiedBank = i;
                smallestViableMemorySize = memorySizes[i];
            }
        }
    }

    return leastOccupiedBank;
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
    auto banks = std::bitset<4>(memoryBanks);
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
