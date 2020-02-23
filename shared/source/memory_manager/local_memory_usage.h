/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <atomic>
#include <memory>

namespace NEO {
class LocalMemoryUsageBankSelector : public NonCopyableOrMovableClass {
  public:
    LocalMemoryUsageBankSelector() = delete;
    LocalMemoryUsageBankSelector(uint32_t banksCount);
    uint32_t getLeastOccupiedBank();
    void reserveOnBanks(uint32_t memoryBanks, uint64_t allocationSize) {
        updateUsageInfo(memoryBanks, allocationSize, true);
    }
    void freeOnBanks(uint32_t memoryBanks, uint64_t allocationSize) {
        updateUsageInfo(memoryBanks, allocationSize, false);
    }

    uint64_t getOccupiedMemorySizeForBank(uint32_t bankIndex) {
        UNRECOVERABLE_IF(bankIndex >= banksCount);
        return memorySizes[bankIndex].load();
    }

  protected:
    uint32_t banksCount = 0;
    std::unique_ptr<std::atomic<uint64_t>[]> memorySizes = nullptr;
    void updateUsageInfo(uint32_t memoryBanks, uint64_t allocationSize, bool reserve);
    void freeOnBank(uint32_t bankIndex, uint64_t allocationSize);
    void reserveOnBank(uint32_t bankIndex, uint64_t allocationSize);
};
} // namespace NEO
