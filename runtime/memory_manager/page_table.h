/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/basic_math.h"
#include "runtime/memory_manager/physical_address_allocator.h"

#include <array>
#include <atomic>
#include <cinttypes>
#include <functional>
#include <memory>
#include <vector>

namespace NEO {

class GraphicsAllocation;

typedef std::function<void(uint64_t addr, size_t size, size_t offset, uint64_t entryBits)> PageWalker;
template <class T, uint32_t level, uint32_t bits = 9>
class PageTable {
  public:
    PageTable(PhysicalAddressAllocator *physicalAddressAllocator) : allocator(physicalAddressAllocator) {
        entries.fill(nullptr);
    };

    virtual ~PageTable() {
        for (auto &e : entries)
            delete e;
    }

    virtual uintptr_t map(uintptr_t vm, size_t size, uint64_t entryBits, uint32_t memoryBank);
    virtual void pageWalk(uintptr_t vm, size_t size, size_t offset, uint64_t entryBits, PageWalker &pageWalker, uint32_t memoryBank);

    static const size_t pageSize = 1 << 12;
    static size_t getBits() {
        return T::getBits() + bits;
    }

  protected:
    std::array<T *, 1 << bits> entries;
    PhysicalAddressAllocator *allocator = nullptr;
};

template <>
inline PageTable<void, 0, 9>::~PageTable() {
}

class PTE : public PageTable<void, 0u> {
  public:
    PTE(PhysicalAddressAllocator *physicalAddressAllocator) : PageTable<void, 0u>(physicalAddressAllocator) {}

    uintptr_t map(uintptr_t vm, size_t size, uint64_t entryBits, uint32_t memoryBank) override;
    void pageWalk(uintptr_t vm, size_t size, size_t offset, uint64_t entryBits, PageWalker &pageWalker, uint32_t memoryBank) override;

    static const uint32_t level = 0;
    static const uint32_t bits = 9;
};

class PDE : public PageTable<class PTE, 1> {
  public:
    PDE(PhysicalAddressAllocator *physicalAddressAllocator) : PageTable<class PTE, 1>(physicalAddressAllocator) {
    }
};

class PDP : public PageTable<class PDE, 2> {
  public:
    PDP(PhysicalAddressAllocator *physicalAddressAllocator) : PageTable<class PDE, 2>(physicalAddressAllocator) {
    }
};

class PML4 : public PageTable<class PDP, 3> {
  public:
    PML4(PhysicalAddressAllocator *physicalAddressAllocator) : PageTable<class PDP, 3>(physicalAddressAllocator) {
    }
};

class PDPE : public PageTable<class PDE, 2, 2> {
  public:
    PDPE(PhysicalAddressAllocator *physicalAddressAllocator) : PageTable<class PDE, 2, 2>(physicalAddressAllocator) {
    }
};
} // namespace NEO
