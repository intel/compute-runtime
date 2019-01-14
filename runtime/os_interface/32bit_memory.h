/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/utilities/heap_allocator.h"
#include <stdint.h>
#include <memory>

namespace OCLRT {
const uintptr_t max32BitAddress = 0xffffffff;
extern bool is32BitOsAllocatorAvailable;
class Allocator32bit {
  protected:
    class OsInternals;

  public:
    Allocator32bit(uint64_t base, uint64_t size);
    Allocator32bit(Allocator32bit::OsInternals *osInternals);
    Allocator32bit();
    MOCKABLE_VIRTUAL ~Allocator32bit();

    uint64_t allocate(size_t &size);
    uintptr_t getBase() const;
    int free(uint64_t ptr, size_t size);

  protected:
    std::unique_ptr<OsInternals> osInternals;
    std::unique_ptr<HeapAllocator> heapAllocator;
    uint64_t base = 0;
    uint64_t size = 0;
};
} // namespace OCLRT
