/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/memory_constants.h"
#include <atomic>

namespace OCLRT {

class PhysicalAddressAllocator {
  public:
    PhysicalAddressAllocator() {
        nextPageAddress.store(initialPageAddress);
    };
    ~PhysicalAddressAllocator() = default;

    uint64_t reservePage(uint32_t memoryBank) {
        auto address = nextPageAddress.fetch_add(MemoryConstants::pageSize);
        return address;
    }

  protected:
    std::atomic<uint64_t> nextPageAddress;
    const uint64_t initialPageAddress = 0x1000;
};

} // namespace OCLRT
