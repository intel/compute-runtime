/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/allocator_helper.h"
#include "runtime/os_interface/linux/drm_limited_range.h"
#include "runtime/os_interface/debug_settings_manager.h"

using namespace OCLRT;

OCLRT::AllocatorLimitedRange::AllocatorLimitedRange(uint64_t base, uint64_t size) : base(base), size(size), heapAllocator(std::make_unique<HeapAllocator>(base, size)) {
}

uint64_t OCLRT::AllocatorLimitedRange::allocate(size_t &size) {
    if (size >= this->size) {
        return 0llu;
    }
    return this->heapAllocator->allocate(size);
}

void OCLRT::AllocatorLimitedRange::free(uint64_t ptr, size_t size) {
    this->heapAllocator->free(ptr, size);
}

uint64_t OCLRT::AllocatorLimitedRange::getBase() const {
    return base;
}
