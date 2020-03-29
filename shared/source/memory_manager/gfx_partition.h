/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/os_interface/os_memory.h"
#include "shared/source/utilities/heap_allocator.h"

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
    HEAP_EXTENDED,

    // Please put new heap indexes above this line
    TOTAL_HEAPS
};

class GfxPartition {
  public:
    GfxPartition();
    MOCKABLE_VIRTUAL ~GfxPartition();

    void init(uint64_t gpuAddressSpace, size_t cpuAddressRangeSizeToReserve, uint32_t rootDeviceIndex, size_t numRootDevices);

    void heapInit(HeapIndex heapIndex, uint64_t base, uint64_t size) {
        getHeap(heapIndex).init(base, size);
    }

    uint64_t heapAllocate(HeapIndex heapIndex, size_t &size) {
        return getHeap(heapIndex).allocate(size);
    }

    void heapFree(HeapIndex heapIndex, uint64_t ptr, size_t size) {
        getHeap(heapIndex).free(ptr, size);
    }

    MOCKABLE_VIRTUAL void freeGpuAddressRange(uint64_t ptr, size_t size);

    uint64_t getHeapBase(HeapIndex heapIndex) {
        return getHeap(heapIndex).getBase();
    }

    uint64_t getHeapLimit(HeapIndex heapIndex) {
        return getHeap(heapIndex).getLimit();
    }

    uint64_t getHeapMinimalAddress(HeapIndex heapIndex) {
        return getHeapBase(heapIndex) + heapGranularity;
    }

    bool isLimitedRange() { return getHeap(HeapIndex::HEAP_SVM).getSize() == 0ull; }

    static const uint64_t heapGranularity = MemoryConstants::pageSize64k;

    static const std::array<HeapIndex, 4> heap32Names;
    static const std::array<HeapIndex, 7> heapNonSvmNames;

  protected:
    void initAdditionalRange(uint64_t gpuAddressSpace, uint64_t &gfxBase, uint64_t &gfxTop, uint32_t rootDeviceIndex, size_t numRootDevices);

    class Heap {
      public:
        Heap() = default;
        void init(uint64_t base, uint64_t size);
        uint64_t getBase() const { return base; }
        uint64_t getSize() const { return size; }
        uint64_t getLimit() const { return size ? base + size - 1 : 0; }
        uint64_t allocate(size_t &size) { return alloc->allocate(size); }
        void free(uint64_t ptr, size_t size) { alloc->free(ptr, size); }

      protected:
        uint64_t base = 0, size = 0;
        std::unique_ptr<HeapAllocator> alloc;
    };

    Heap &getHeap(HeapIndex heapIndex) {
        return heaps[static_cast<uint32_t>(heapIndex)];
    }

    std::array<Heap, static_cast<uint32_t>(HeapIndex::TOTAL_HEAPS)> heaps;

    OSMemory::ReservedCpuAddressRange reservedCpuAddressRange;
    std::unique_ptr<OSMemory> osMemory;
};

} // namespace NEO
