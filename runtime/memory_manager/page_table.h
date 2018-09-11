/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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

namespace OCLRT {

class GraphicsAllocation;

class PageTableHelper {
  public:
    static uint32_t getMemoryBankIndex(GraphicsAllocation &allocation) {
        return memoryBankNotSpecified;
    }

    static const uint32_t memoryBankNotSpecified = 0;
};

typedef std::function<void(uint64_t addr, size_t size, size_t offset)> PageWalker;
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

    virtual uintptr_t map(uintptr_t vm, size_t size, uint32_t memoryBank);
    virtual void pageWalk(uintptr_t vm, size_t size, size_t offset, PageWalker &pageWalker, uint32_t memoryBank);

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

    uintptr_t map(uintptr_t vm, size_t size, uint32_t memoryBank) override;
    void pageWalk(uintptr_t vm, size_t size, size_t offset, PageWalker &pageWalker, uint32_t memoryBank) override;

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
} // namespace OCLRT
