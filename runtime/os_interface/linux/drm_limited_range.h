/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/utilities/heap_allocator.h"
#include <stdint.h>
#include <memory>
#include <sys/mman.h>

namespace OCLRT {
class AllocatorLimitedRange {
  public:
    AllocatorLimitedRange(uint64_t base, uint64_t size);
    AllocatorLimitedRange() = delete;
    ~AllocatorLimitedRange() = default;

    uint64_t allocate(size_t &size);
    void free(uint64_t ptr, size_t size);
    uint64_t getBase() const;

  protected:
    uint64_t base = 0;
    uint64_t size = 0;
    std::unique_ptr<HeapAllocator> heapAllocator;
};
} // namespace OCLRT
