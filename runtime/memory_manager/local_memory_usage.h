/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/properties_helper.h"

#include <atomic>
#include <memory>

namespace NEO {
class LocalMemoryUsageBankSelector : public NonCopyableOrMovableClass {
  public:
    LocalMemoryUsageBankSelector() = delete;
    LocalMemoryUsageBankSelector(uint32_t banksCount);
    uint32_t getLeastOccupiedBank();
    void freeOnBank(uint32_t bankIndex, uint64_t allocationSize);
    void reserveOnBank(uint32_t bankIndex, uint64_t allocationSize);

    MOCKABLE_VIRTUAL uint64_t getOccupiedMemorySizeForBank(uint32_t bankIndex) {
        UNRECOVERABLE_IF(bankIndex >= banksCount);
        return memorySizes[bankIndex].load();
    }

  protected:
    uint32_t banksCount = 0;
    std::unique_ptr<std::atomic<uint64_t>[]> memorySizes = nullptr;
};
} // namespace NEO
