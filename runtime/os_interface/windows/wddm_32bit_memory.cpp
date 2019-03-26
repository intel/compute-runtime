/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/aligned_memory.h"
#include "runtime/os_interface/32bit_memory.h"
using namespace NEO;

bool NEO::is32BitOsAllocatorAvailable = is64bit;

class Allocator32bit::OsInternals {
  public:
    void *allocatedRange;
};

Allocator32bit::Allocator32bit(uint64_t base, uint64_t size) : base(base), size(size) {
    heapAllocator = std::make_unique<HeapAllocator>(base, size);
}

NEO::Allocator32bit::Allocator32bit() {
    size_t sizeToMap = 100 * 4096;
    this->base = (uint64_t)alignedMalloc(sizeToMap, 4096);
    osInternals = std::make_unique<OsInternals>();
    osInternals->allocatedRange = (void *)((uintptr_t)this->base);

    heapAllocator = std::make_unique<HeapAllocator>(this->base, sizeToMap);
}

NEO::Allocator32bit::~Allocator32bit() {
    if (this->osInternals.get() != nullptr) {
        alignedFree(this->osInternals->allocatedRange);
    }
}

uint64_t Allocator32bit::allocate(size_t &size) {
    if (size >= 0xfffff000)
        return 0llu;
    return this->heapAllocator->allocate(size);
}

int Allocator32bit::free(uint64_t ptr, size_t size) {
    this->heapAllocator->free(ptr, size);
    return 0;
}

uintptr_t Allocator32bit::getBase() const {
    return (uintptr_t)base;
}
