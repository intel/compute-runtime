/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/debug_helpers.h"
#include "runtime/memory_manager/memory_banks.h"
#include "runtime/memory_manager/memory_constants.h"
#include <atomic>

namespace OCLRT {

class PhysicalAddressAllocator {
  public:
    PhysicalAddressAllocator() {
        mainAllocator.store(initialPageAddress);
    }

    virtual ~PhysicalAddressAllocator() = default;

    virtual uint64_t reservePage(uint32_t memoryBank) {
        UNRECOVERABLE_IF(memoryBank != MemoryBanks::MainBank);
        return mainAllocator.fetch_add(MemoryConstants::pageSize);
    }

  protected:
    std::atomic<uint64_t> mainAllocator;
    const uint64_t initialPageAddress = 0x1000;
};

} // namespace OCLRT
