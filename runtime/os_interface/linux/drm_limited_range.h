/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/basic_math.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/utilities/heap_allocator.h"

#include <memory>
#include <stdint.h>
#include <sys/mman.h>

namespace NEO {
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
} // namespace NEO
