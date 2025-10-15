/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/os_memory.h"

#include <array>

namespace NEO {
class HeapAllocator;

enum class HeapIndex : uint32_t {
    heapInternalDeviceMemory = 0u,
    heapInternal = 1u,
    heapExternalDeviceMemory = 2u,
    heapExternal = 3u,
    heapStandard,
    heapStandard64KB,
    heapStandard2MB,
    heapSvm,
    heapExtended,
    heapExternalFrontWindow,
    heapExternalDeviceFrontWindow,
    heapInternalFrontWindow,
    heapInternalDeviceFrontWindow,
    heapExtendedHost,

    // Please put new heap indexes above this line
    totalHeaps
};

class GfxPartition {
  public:
    GfxPartition(OSMemory::ReservedCpuAddressRange &reservedCpuAddressRangeForNonSvmHeaps);
    MOCKABLE_VIRTUAL ~GfxPartition();

    MOCKABLE_VIRTUAL bool init(uint64_t gpuAddressSpace, size_t cpuAddressRangeSizeToReserve, uint32_t rootDeviceIndex, size_t numRootDevices, bool useExternalFrontWindowPool, uint64_t systemMemorySize, uint64_t gfxTop);

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

    MOCKABLE_VIRTUAL uint64_t heapAllocateWithStartAddressHint(const uint64_t requiredStartAddress, HeapIndex heapIndex, size_t &size) {
        return getHeap(heapIndex).allocateWithStartAddressHint(requiredStartAddress, size);
    }

    MOCKABLE_VIRTUAL uint64_t heapAllocateWithCustomAlignment(HeapIndex heapIndex, size_t &size, size_t alignment) {
        return getHeap(heapIndex).allocateWithCustomAlignment(size, alignment);
    }

    MOCKABLE_VIRTUAL uint64_t heapAllocateWithCustomAlignmentWithStartAddressHint(const uint64_t requiredStartAddress, HeapIndex heapIndex, size_t &size, size_t alignment) {
        return getHeap(heapIndex).allocateWithCustomAlignmentWithStartAddressHint(requiredStartAddress, size, alignment);
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

    size_t getHeapAllocationAlignment(HeapIndex heapIndex) {
        return getHeap(heapIndex).getAllocAlignment();
    }

    bool isHeapInitialized(HeapIndex heapIndex) {
        return getHeap(heapIndex).isInitialized();
    }

    uint64_t getHeapMinimalAddress(HeapIndex heapIndex);

    MOCKABLE_VIRTUAL bool getHeapIndexAndPageSizeBasedOnAddress(uint64_t ptr, HeapIndex &heapIndex, size_t &pageSize) {
        for (size_t index = 0; index < heaps.size(); ++index) {

            if (!isHeapInitialized(static_cast<HeapIndex>(index))) {
                continue;
            }
            if (isAddressInHeapRange(static_cast<HeapIndex>(index), ptr)) {
                heapIndex = static_cast<HeapIndex>(index);
                pageSize = getHeapAllocationAlignment(heapIndex);
                return true;
            }
        }
        return false;
    }

    bool isLimitedRange() { return getHeap(HeapIndex::heapSvm).getSize() == 0ull; }

    static bool isAnyHeap32(HeapIndex heapIndex) {
        if ((heapIndex >= HeapIndex::heapInternalDeviceMemory && heapIndex <= HeapIndex::heapExternal) ||
            (heapIndex >= HeapIndex::heapExternalFrontWindow && heapIndex <= HeapIndex::heapInternalDeviceFrontWindow)) {
            return true;
        }
        return false;
    }

    static constexpr uint64_t heapGranularity = MemoryConstants::pageSize64k;
    static constexpr uint64_t heapGranularity64k = MemoryConstants::pageSize64k;
    static constexpr uint64_t heapGranularity2MB = 2 * MemoryConstants::megaByte;
    static constexpr size_t externalFrontWindowPoolSize = 2 * MemoryConstants::pageSize64k;
    static constexpr size_t internalFrontWindowPoolSize = 1 * MemoryConstants::megaByte;

    static const std::array<HeapIndex, 4> heap32Names;
    static const std::array<HeapIndex, 8> heapNonSvmNames;

  protected:
    bool initAdditionalRange(uint32_t cpuAddressWidth, uint64_t gpuAddressSpace, uint64_t &gfxBase, uint64_t &gfxTop, uint32_t rootDeviceIndex, uint64_t systemMemorySize, size_t numRootDevices);

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
        size_t getAllocAlignment() const;
        uint64_t allocate(size_t &size);
        uint64_t allocateWithStartAddressHint(const uint64_t requiredStartAddress, size_t &size);
        uint64_t allocateWithCustomAlignment(size_t &sizeToAllocate, size_t alignment);
        uint64_t allocateWithCustomAlignmentWithStartAddressHint(const uint64_t requiredStartAddress, size_t &sizeToAllocate, size_t alignment);
        void free(uint64_t ptr, size_t size);
        bool isInitialized() const { return initialized; }

      protected:
        uint64_t base = 0, size = 0;
        std::unique_ptr<HeapAllocator> alloc;
        bool initialized = false;
    };

    Heap &getHeap(HeapIndex heapIndex) {
        return heaps[static_cast<uint32_t>(heapIndex)];
    }

    bool isAddressInHeapRange(HeapIndex heapIndex, uint64_t ptr) {
        return (ptr >= getHeap(heapIndex).getBase()) && (ptr <= getHeap(heapIndex).getLimit());
    }

    std::array<Heap, static_cast<uint32_t>(HeapIndex::totalHeaps)> heaps;

    OSMemory::ReservedCpuAddressRange &reservedCpuAddressRangeForNonSvmHeaps;
    OSMemory::ReservedCpuAddressRange reservedCpuAddressRangeForHeapExtended{};
    std::unique_ptr<OSMemory> osMemory;
};
} // namespace NEO
