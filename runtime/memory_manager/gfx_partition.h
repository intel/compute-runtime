/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/utilities/heap_allocator.h"

#include <array>

namespace NEO {

enum class HeapIndex : uint32_t {
    HEAP_INTERNAL_DEVICE_MEMORY = 0u,
    HEAP_INTERNAL = 1u,
    HEAP_EXTERNAL_DEVICE_MEMORY = 2u,
    HEAP_EXTERNAL = 3u,
    HEAP_STANDARD,
    HEAP_STANDARD64KB,
    HEAP_SVM,

    // Please put new heap indexes above this line
    TOTAL_HEAPS
};

constexpr auto internalHeapIndex = is32bit ? HeapIndex::HEAP_INTERNAL : HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY;

class GfxPartition {
  public:
    GfxPartition() {}

    void init(uint64_t gpuAddressSpace);

    void heapInit(HeapIndex heapIndex, uint64_t base, uint64_t size) {
        getHeap(heapIndex).init(base, size);
    }

    uint64_t heapAllocate(HeapIndex heapIndex, size_t &size) {
        return getHeap(heapIndex).allocate(size);
    }

    void heapFree(HeapIndex heapIndex, uint64_t ptr, size_t size) {
        getHeap(heapIndex).free(ptr, size);
    }

    uint64_t getHeapBase(HeapIndex heapIndex) {
        return getHeap(heapIndex).getBase();
    }

    uint64_t getHeapLimit(HeapIndex heapIndex) {
        return getHeap(heapIndex).getBase() + getHeap(heapIndex).getSize() - 1;
    }

    uint64_t getHeapMinimalAddress(HeapIndex heapIndex) {
        return getHeapBase(heapIndex) + heapGranularity;
    }

    static const uint64_t heapGranularity = MemoryConstants::pageSize64k;

    static const std::array<HeapIndex, 4> heap32Names;

  protected:
    class Heap {
      public:
        Heap() = default;
        uint64_t getBase() const { return base; }
        uint64_t getSize() const { return size; }
        void init(uint64_t base, uint64_t size) {
            this->base = base;
            this->size = size;
            alloc = std::make_unique<HeapAllocator>(base + heapGranularity, size ? size - heapGranularity : 0ull);
        }
        uint64_t allocate(size_t &size) { return alloc->allocate(size); }
        void free(uint64_t ptr, size_t size) { alloc->free(ptr, size); }

      protected:
        uint64_t base = 0, size = 0;
        std::unique_ptr<HeapAllocator> alloc;
    };

    Heap &getHeap(HeapIndex heapIndex) {
        return heap[static_cast<uint32_t>(heapIndex)];
    }

    std::array<Heap, static_cast<uint32_t>(HeapIndex::TOTAL_HEAPS)> heap;
};

} // namespace NEO
