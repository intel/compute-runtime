/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/heap_assigner.h"
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
    HEAP_STANDARD2MB,
    HEAP_SVM,
    HEAP_EXTENDED,
    HEAP_EXTERNAL_FRONT_WINDOW,
    HEAP_EXTERNAL_DEVICE_FRONT_WINDOW,
    HEAP_INTERNAL_FRONT_WINDOW,
    HEAP_INTERNAL_DEVICE_FRONT_WINDOW,

    // Please put new heap indexes above this line
    TOTAL_HEAPS
};

class GfxPartition {
  public:
    GfxPartition(OSMemory::ReservedCpuAddressRange &sharedReservedCpuAddressRange);
    MOCKABLE_VIRTUAL ~GfxPartition();

    bool init(uint64_t gpuAddressSpace, size_t cpuAddressRangeSizeToReserve, uint32_t rootDeviceIndex, size_t numRootDevices) {
        return init(gpuAddressSpace, cpuAddressRangeSizeToReserve, rootDeviceIndex, numRootDevices, false);
    }
    MOCKABLE_VIRTUAL bool init(uint64_t gpuAddressSpace, size_t cpuAddressRangeSizeToReserve, uint32_t rootDeviceIndex, size_t numRootDevices, bool useExternalFrontWindowPool);

    void heapInit(HeapIndex heapIndex, uint64_t base, uint64_t size) {
        getHeap(heapIndex).init(base, size, MemoryConstants::pageSize);
    }

    void heapInitWithAllocationAlignment(HeapIndex heapIndex, uint64_t base, uint64_t size, size_t allocationAlignment) {
        getHeap(heapIndex).init(base, size, allocationAlignment);
    }

    void heapInitExternalWithFrontWindow(HeapIndex heapIndex, uint64_t base, uint64_t size) {
        getHeap(heapIndex).initExternalWithFrontWindow(base, size);
    }

    void heapInitWithFrontWindow(HeapIndex heapIndex, uint64_t base, uint64_t size, uint64_t frontWindowSize) {
        getHeap(heapIndex).initWithFrontWindow(base, size, frontWindowSize);
    }

    void heapInitFrontWindow(HeapIndex heapIndex, uint64_t base, uint64_t size) {
        getHeap(heapIndex).initFrontWindow(base, size);
    }

    MOCKABLE_VIRTUAL uint64_t heapAllocate(HeapIndex heapIndex, size_t &size) {
        return getHeap(heapIndex).allocate(size);
    }

    MOCKABLE_VIRTUAL uint64_t heapAllocateWithCustomAlignment(HeapIndex heapIndex, size_t &size, size_t alignment) {
        return getHeap(heapIndex).allocateWithCustomAlignment(size, alignment);
    }

    MOCKABLE_VIRTUAL void heapFree(HeapIndex heapIndex, uint64_t ptr, size_t size) {
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
        if (heapIndex == HeapIndex::HEAP_SVM ||
            heapIndex == HeapIndex::HEAP_EXTERNAL_DEVICE_FRONT_WINDOW ||
            heapIndex == HeapIndex::HEAP_EXTERNAL_FRONT_WINDOW ||
            heapIndex == HeapIndex::HEAP_INTERNAL_DEVICE_FRONT_WINDOW ||
            heapIndex == HeapIndex::HEAP_INTERNAL_FRONT_WINDOW) {
            return getHeapBase(heapIndex);
        } else {
            if ((heapIndex == HeapIndex::HEAP_EXTERNAL ||
                 heapIndex == HeapIndex::HEAP_EXTERNAL_DEVICE_MEMORY) &&
                (getHeapLimit(HeapAssigner::mapExternalWindowIndex(heapIndex)) != 0)) {
                return getHeapBase(heapIndex) + GfxPartition::externalFrontWindowPoolSize;
            } else if (heapIndex == HeapIndex::HEAP_INTERNAL ||
                       heapIndex == HeapIndex::HEAP_INTERNAL_DEVICE_MEMORY) {
                return getHeapBase(heapIndex) + GfxPartition::internalFrontWindowPoolSize;
            } else if (heapIndex == HeapIndex::HEAP_STANDARD2MB) {
                return getHeapBase(heapIndex) + GfxPartition::heapGranularity2MB;
            }
            return getHeapBase(heapIndex) + GfxPartition::heapGranularity;
        }
    }

    bool isLimitedRange() { return getHeap(HeapIndex::HEAP_SVM).getSize() == 0ull; }

    static constexpr uint64_t heapGranularity = MemoryConstants::pageSize64k;
    static constexpr uint64_t heapGranularity2MB = 2 * MemoryConstants::megaByte;
    static constexpr size_t externalFrontWindowPoolSize = 16 * MemoryConstants::megaByte;
    static constexpr size_t internalFrontWindowPoolSize = 1 * MemoryConstants::megaByte;

    static const std::array<HeapIndex, 4> heap32Names;
    static const std::array<HeapIndex, 8> heapNonSvmNames;

  protected:
    bool initAdditionalRange(uint32_t cpuAddressWidth, uint64_t gpuAddressSpace, uint64_t &gfxBase, uint64_t &gfxTop, uint32_t rootDeviceIndex, size_t numRootDevices);

    class Heap {
      public:
        Heap() = default;
        void init(uint64_t base, uint64_t size, size_t allocationAlignment);
        void initExternalWithFrontWindow(uint64_t base, uint64_t size);
        void initWithFrontWindow(uint64_t base, uint64_t size, uint64_t frontWindowSize);
        void initFrontWindow(uint64_t base, uint64_t size);
        uint64_t getBase() const { return base; }
        uint64_t getSize() const { return size; }
        uint64_t getLimit() const { return size ? base + size - 1 : 0; }
        uint64_t allocate(size_t &size) { return alloc->allocate(size); }
        uint64_t allocateWithCustomAlignment(size_t &sizeToAllocate, size_t alignment) { return alloc->allocateWithCustomAlignment(sizeToAllocate, alignment); }
        void free(uint64_t ptr, size_t size) { alloc->free(ptr, size); }

      protected:
        uint64_t base = 0, size = 0;
        std::unique_ptr<HeapAllocator> alloc;
    };

    Heap &getHeap(HeapIndex heapIndex) {
        return heaps[static_cast<uint32_t>(heapIndex)];
    }

    std::array<Heap, static_cast<uint32_t>(HeapIndex::TOTAL_HEAPS)> heaps;

    OSMemory::ReservedCpuAddressRange &reservedCpuAddressRange;
    std::unique_ptr<OSMemory> osMemory;
};

} // namespace NEO
