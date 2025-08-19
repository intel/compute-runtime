/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/gfx_partition.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/heap_assigner.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/source/utilities/heap_allocator.h"

namespace NEO {

const std::array<HeapIndex, 4> GfxPartition::heap32Names{{HeapIndex::heapInternalDeviceMemory,
                                                          HeapIndex::heapInternal,
                                                          HeapIndex::heapExternalDeviceMemory,
                                                          HeapIndex::heapExternal}};

const std::array<HeapIndex, 8> GfxPartition::heapNonSvmNames{{HeapIndex::heapInternalDeviceMemory,
                                                              HeapIndex::heapInternal,
                                                              HeapIndex::heapExternalDeviceMemory,
                                                              HeapIndex::heapExternal,
                                                              HeapIndex::heapStandard,
                                                              HeapIndex::heapStandard64KB,
                                                              HeapIndex::heapStandard2MB,
                                                              HeapIndex::heapExtended}};

static void reserveLow48BitRangeWithRetry(OSMemory *osMemory, OSMemory::ReservedCpuAddressRange &reservedCpuAddressRange, size_t numRootDevices) {
    uint64_t reservationSize = numRootDevices * MemoryConstants::teraByte;
    constexpr uint64_t minimalReservationSize = 32 * MemoryConstants::gigaByte;

    while (reservationSize >= minimalReservationSize) {
        // With no base address being specified OS always reserve memory in [0x000000000000-0x7FFFFFFFFFFF] range
        reservedCpuAddressRange = osMemory->reserveCpuAddressRange(static_cast<size_t>(reservationSize), GfxPartition::heapGranularity);

        if (reservedCpuAddressRange.alignedPtr) {
            break;
        }

        // Oops... Try again with smaller chunk
        reservationSize = alignDown(static_cast<uint64_t>(reservationSize * 0.9), MemoryConstants::pageSize64k);
    };
}

static void reserveRangeWithMemoryMapsParse(OSMemory *osMemory, OSMemory::ReservedCpuAddressRange &reservedCpuAddressRange, uint64_t areaBase, uint64_t areaTop, uint64_t reservationSize) {
    uint64_t reservationBase = areaBase;

    reservedCpuAddressRange = osMemory->reserveCpuAddressRange(reinterpret_cast<void *>(reservationBase), static_cast<size_t>(reservationSize), MemoryConstants::pageSize64k);

    if (reservedCpuAddressRange.alignedPtr != nullptr) {
        uint64_t alignedPtrU64 = castToUint64(reservedCpuAddressRange.alignedPtr);
        if (alignedPtrU64 >= areaBase && alignedPtrU64 + reservationSize < areaTop) {
            return;
        } else {
            osMemory->releaseCpuAddressRange(reservedCpuAddressRange);
            reservedCpuAddressRange.alignedPtr = nullptr;
        }
    }

    OSMemory::MemoryMaps memoryMaps;
    osMemory->getMemoryMaps(memoryMaps);

    for (size_t i = 0; reservationBase < areaTop && i < memoryMaps.size(); ++i) {
        if (memoryMaps[i].end < areaBase) {
            continue;
        }

        if (memoryMaps[i].start - reservationBase >= reservationSize) {
            break;
        }
        reservationBase = memoryMaps[i].end;
    }

    if (reservationBase + reservationSize < areaTop) {
        reservedCpuAddressRange = osMemory->reserveCpuAddressRange(reinterpret_cast<void *>(reservationBase), static_cast<size_t>(reservationSize), MemoryConstants::pageSize64k);
    }
}

static void reserveHigh48BitRangeWithMemoryMapsParse(OSMemory *osMemory, OSMemory::ReservedCpuAddressRange &reservedCpuAddressRange, size_t numRootDevices) {
    constexpr uint64_t high48BitAreaBase = maxNBitValue(47) + 1; // 0x800000000000
    constexpr uint64_t high48BitAreaTop = maxNBitValue(48);      // 0xFFFFFFFFFFFF
    uint64_t reservationSize = numRootDevices * MemoryConstants::teraByte;
    reserveRangeWithMemoryMapsParse(osMemory, reservedCpuAddressRange, high48BitAreaBase, high48BitAreaTop, reservationSize);
}

static void reserve57BitRangeWithMemoryMapsParse(OSMemory *osMemory, OSMemory::ReservedCpuAddressRange &reservedCpuAddressRange, uint64_t reservationSize) {
    constexpr uint64_t areaBase = maxNBitValue(48) + 1;
    constexpr uint64_t areaTop = maxNBitValue(56);
    reserveRangeWithMemoryMapsParse(osMemory, reservedCpuAddressRange, areaBase, areaTop, reservationSize);
}

GfxPartition::GfxPartition(OSMemory::ReservedCpuAddressRange &reservedCpuAddressRangeForNonSvmHeaps) : reservedCpuAddressRangeForNonSvmHeaps(reservedCpuAddressRangeForNonSvmHeaps), osMemory(OSMemory::create()) {}

GfxPartition::~GfxPartition() {
    osMemory->releaseCpuAddressRange(reservedCpuAddressRangeForNonSvmHeaps);
    reservedCpuAddressRangeForNonSvmHeaps = {};
    osMemory->releaseCpuAddressRange(reservedCpuAddressRangeForHeapExtended);
}

void GfxPartition::Heap::init(uint64_t base, uint64_t size, size_t allocationAlignment) {
    this->base = base;
    this->size = size;

    auto heapGranularity = GfxPartition::heapGranularity;
    if (allocationAlignment > heapGranularity) {
        heapGranularity = GfxPartition::heapGranularity2MB;
    }

    // Exclude very first and very last 64K from GPU address range allocation
    if (size > 2 * heapGranularity) {
        size -= 2 * heapGranularity;
    }

    alloc = std::make_unique<HeapAllocator>(base + heapGranularity, size, allocationAlignment);
}

void GfxPartition::Heap::initExternalWithFrontWindow(uint64_t base, uint64_t size) {
    this->base = base;
    this->size = size;

    size -= GfxPartition::heapGranularity;

    alloc = std::make_unique<HeapAllocator>(base, size, MemoryConstants::pageSize, 0u);
}

void GfxPartition::Heap::initWithFrontWindow(uint64_t base, uint64_t size, uint64_t frontWindowSize) {
    this->base = base;
    this->size = size;

    // Exclude very very last 64K from GPU address range allocation
    size -= GfxPartition::heapGranularity;
    size -= frontWindowSize;

    alloc = std::make_unique<HeapAllocator>(base + frontWindowSize, size, MemoryConstants::pageSize);
}

void GfxPartition::Heap::initFrontWindow(uint64_t base, uint64_t size) {
    this->base = base;
    this->size = size;

    alloc = std::make_unique<HeapAllocator>(base, size, MemoryConstants::pageSize, 0u);
}

uint64_t GfxPartition::Heap::allocate(size_t &size) {
    return alloc->allocate(size);
}

uint64_t GfxPartition::Heap::allocateWithCustomAlignment(size_t &sizeToAllocate, size_t alignment) {
    return alloc->allocateWithCustomAlignment(sizeToAllocate, alignment);
}

void GfxPartition::Heap::free(uint64_t ptr, size_t size) {
    alloc->free(ptr, size);
}

void GfxPartition::freeGpuAddressRange(uint64_t ptr, size_t size) {
    for (auto heapName : GfxPartition::heapNonSvmNames) {
        auto &heap = getHeap(heapName);
        if ((ptr > heap.getBase()) && ((ptr + size) < heap.getLimit())) {
            heap.free(ptr, size);
            break;
        }
    }
}

uint64_t GfxPartition::getHeapMinimalAddress(HeapIndex heapIndex) {
    if (heapIndex == HeapIndex::heapSvm ||
        heapIndex == HeapIndex::heapExternalDeviceFrontWindow ||
        heapIndex == HeapIndex::heapExternalFrontWindow ||
        heapIndex == HeapIndex::heapInternalDeviceFrontWindow ||
        heapIndex == HeapIndex::heapInternalFrontWindow) {
        return getHeapBase(heapIndex);
    } else {
        if ((heapIndex == HeapIndex::heapExternal ||
             heapIndex == HeapIndex::heapExternalDeviceMemory) &&
            (getHeapLimit(HeapAssigner::mapExternalWindowIndex(heapIndex)) != 0)) {
            return getHeapBase(heapIndex) + GfxPartition::externalFrontWindowPoolSize;
        } else if (heapIndex == HeapIndex::heapInternal ||
                   heapIndex == HeapIndex::heapInternalDeviceMemory) {
            return getHeapBase(heapIndex) + GfxPartition::internalFrontWindowPoolSize;
        } else if (heapIndex == HeapIndex::heapStandard2MB) {
            return getHeapBase(heapIndex) + GfxPartition::heapGranularity2MB;
        }
        return getHeapBase(heapIndex) + GfxPartition::heapGranularity;
    }
}

bool GfxPartition::init(uint64_t gpuAddressSpace, size_t cpuAddressRangeSizeToReserve, uint32_t rootDeviceIndex, size_t numRootDevices, bool useExternalFrontWindowPool, uint64_t systemMemorySize, uint64_t gfxTop) {

    /*
     * I. 64-bit builds:
     *
     *   1) 48-bit Full Range SVM gfx layout:
     *
     *                   SVM                  H0   H1   H2   H3      STANDARD      STANDARD64K
     *   |__________________________________|____|____|____|____|________________|______________|
     *   |                                  |    |    |    |    |                |              |
     *   |                                gfxBase                                             gfxTop
     *  0x0                          0x0000800000000000                               0x0000FFFFFFFFFFFF
     *
     *
     *   2) 47-bit Full Range SVM gfx layout:
     *
     *                             gfxSize = 2^47 / 4 = 0x200000000000
     *                      ________________________________________________
     *                     /                                                \
     *           SVM      / H0   H1   H2   H3      STANDARD      STANDARD64K \      SVM
     *   |________________|____|____|____|____|________________|______________|_______________|
     *   |                |    |    |    |    |                |              |               |
     *   |              gfxBase                                             gfxTop            |
     *  0x0    reserveCpuAddressRange(gfxSize)                                      0x00007FFFFFFFFFFF
     *   \_____________________________________ SVM _________________________________________/
     *
     *
     *
     *   3) Limited Range gfx layout (no SVM):
     *
     *     H0   H1   H2   H3        STANDARD          STANDARD64K
     *   |____|____|____|____|____________________|__________________|
     *   |    |    |    |    |                    |                  |
     * gfxBase                                                    gfxTop
     *  0x0                                                    0xFFF...FFF < 47 bit
     *
     *
     * II. 32-bit builds:
     *
     *   1) 32-bit Full Range SVM gfx layout:
     *
     *      SVM    H0   H1   H2   H3      STANDARD      STANDARD64K
     *   |_______|____|____|____|____|________________|______________|
     *   |       |    |    |    |    |                |              |
     *   |     gfxBase                                             gfxTop
     *  0x0  0x100000000                                       gpuAddressSpace
     */

    uint64_t gfxBase = 0x0ull;
    const uint64_t gfxHeap32Size = 4 * MemoryConstants::gigaByte;

    if (is32bit) {
        gfxBase = maxNBitValue(32) + 1;
        heapInit(HeapIndex::heapSvm, 0ull, gfxBase);
    } else {
        auto cpuVirtualAddressSize = CpuInfo::getInstance().getVirtualAddressSize();
        if (cpuVirtualAddressSize == 48 && gpuAddressSpace == maxNBitValue(48)) {
            gfxBase = maxNBitValue(48 - 1) + 1;
            heapInit(HeapIndex::heapSvm, 0ull, gfxBase);
        } else if (gpuAddressSpace == maxNBitValue(47)) {
            if (reservedCpuAddressRangeForNonSvmHeaps.alignedPtr == nullptr) {
                if (cpuAddressRangeSizeToReserve == 0) {
                    return false;
                }
                reservedCpuAddressRangeForNonSvmHeaps = osMemory->reserveCpuAddressRange(cpuAddressRangeSizeToReserve, GfxPartition::heapGranularity);
                if (reservedCpuAddressRangeForNonSvmHeaps.originalPtr == nullptr) {
                    return false;
                }
                if (!isAligned<GfxPartition::heapGranularity>(reservedCpuAddressRangeForNonSvmHeaps.alignedPtr)) {
                    return false;
                }
            }
            gfxBase = reinterpret_cast<uint64_t>(reservedCpuAddressRangeForNonSvmHeaps.alignedPtr);
            gfxTop = gfxBase + cpuAddressRangeSizeToReserve;
            heapInit(HeapIndex::heapSvm, 0ull, gpuAddressSpace + 1);
        } else if (gpuAddressSpace < maxNBitValue(47)) {
            gfxBase = 0ull;
            heapInit(HeapIndex::heapSvm, 0ull, 0ull);
        } else {
            if (!initAdditionalRange(cpuVirtualAddressSize, gpuAddressSpace, gfxBase, gfxTop, rootDeviceIndex, systemMemorySize, numRootDevices)) {
                return false;
            }
        }
    }

    for (auto heap : GfxPartition::heap32Names) {
        if (useExternalFrontWindowPool && HeapAssigner::heapTypeExternalWithFrontWindowPool(heap)) {
            heapInitExternalWithFrontWindow(heap, gfxBase, gfxHeap32Size);
            size_t externalFrontWindowSize = GfxPartition::externalFrontWindowPoolSize;
            auto allocation = heapAllocate(heap, externalFrontWindowSize);
            heapInitExternalWithFrontWindow(HeapAssigner::mapExternalWindowIndex(heap), allocation,
                                            externalFrontWindowSize);
        } else if (HeapAssigner::isInternalHeap(heap)) {
            heapInitWithFrontWindow(heap, gfxBase, gfxHeap32Size, GfxPartition::internalFrontWindowPoolSize);
            heapInitFrontWindow(HeapAssigner::mapInternalWindowIndex(heap), gfxBase, GfxPartition::internalFrontWindowPoolSize);
        } else {
            heapInit(heap, gfxBase, gfxHeap32Size);
        }
        gfxBase += gfxHeap32Size;
    }

    constexpr uint32_t numStandardHeaps = static_cast<uint32_t>(HeapIndex::heapStandard2MB) - static_cast<uint32_t>(HeapIndex::heapStandard) + 1;
    constexpr uint64_t maxStandardHeapGranularity = std::max(GfxPartition::heapGranularity, GfxPartition::heapGranularity2MB);

    gfxBase = alignUp(gfxBase, maxStandardHeapGranularity);
    uint64_t maxStandardHeapSize = alignDown((gfxTop - gfxBase) / numStandardHeaps, maxStandardHeapGranularity);
    uint64_t maxStandard64HeapSize = maxStandardHeapSize;
    uint64_t maxStandard2MBHeapSize = maxStandardHeapSize;

    if (gpuAddressSpace == maxNBitValue(57)) {
        maxStandardHeapSize *= 2;
        maxStandard64HeapSize /= 2;
        maxStandard2MBHeapSize /= 2;
    }

    auto gfxStandardSize = maxStandardHeapSize;
    heapInit(HeapIndex::heapStandard, gfxBase, gfxStandardSize);
    DEBUG_BREAK_IF(!isAligned<GfxPartition::heapGranularity>(getHeapBase(HeapIndex::heapStandard)));

    gfxBase += maxStandardHeapSize;

    // Split HEAP_STANDARD64K among root devices
    auto gfxStandard64KBSize = alignDown(maxStandard64HeapSize / numRootDevices, GfxPartition::heapGranularity);
    heapInitWithAllocationAlignment(HeapIndex::heapStandard64KB, gfxBase + rootDeviceIndex * gfxStandard64KBSize, gfxStandard64KBSize, MemoryConstants::pageSize64k);
    DEBUG_BREAK_IF(!isAligned<GfxPartition::heapGranularity>(getHeapBase(HeapIndex::heapStandard64KB)));

    gfxBase += maxStandard64HeapSize;

    // Split HEAP_STANDARD2MB among root devices
    auto gfxStandard2MBSize = alignDown(maxStandard2MBHeapSize / numRootDevices, GfxPartition::heapGranularity2MB);
    heapInitWithAllocationAlignment(HeapIndex::heapStandard2MB, gfxBase + rootDeviceIndex * gfxStandard2MBSize, gfxStandard2MBSize, 2 * MemoryConstants::megaByte);
    DEBUG_BREAK_IF(!isAligned<GfxPartition::heapGranularity2MB>(getHeapBase(HeapIndex::heapStandard2MB)));

    return true;
}

bool GfxPartition::initAdditionalRange(uint32_t cpuVirtualAddressSize, uint64_t gpuAddressSpace, uint64_t &gfxBase, uint64_t &gfxTop, uint32_t rootDeviceIndex, uint64_t systemMemorySize, size_t numRootDevices) {
    /*
     * 57-bit Full Range SVM gfx layout:
     *
     *                                gfxSize = 256GB(48b)/1TB(57b)                                 2^48 = 0x1_0000_0000_0000          (Not Used Now)
     *                      ________________________________________________                     _______________________________     ___________________
     *                     /                                                \                   /                               \   /                   \
     *           SVM      / H0   H1   H2   H3      STANDARD      STANDARD64K \      SVM        /          HEAP_EXTENDED          \ /                     \
     *   |________________|____|____|____|____|________________|______________|_______________|___________________________________|______________ ..... __|
     *   |                |    |    |    |    |                |              |               |                                   |                       |
     *   |              gfxBase                                     gfxTop < 0xFFFFFFFFFFFF   |                                   |                       |
     *  0x0  reserveCpuAddressRange(gfxSize) < 0xFFFFFFFFFFFF - gfxSize             0x100_0000_0000_0000(57b)           0x100_FFFF_FFFF_FFFF    0x1FF_FFFF_FFFF_FFFF
     *   \_____________________________________ SVM _________________________________________/
     *
     */

    // We are here means either CPU VA or GPU VA or both are 57 bit
    if (cpuVirtualAddressSize != 57 && cpuVirtualAddressSize != 48) {
        return false;
    }

    if (gpuAddressSpace != maxNBitValue(57) && gpuAddressSpace != maxNBitValue(48)) {
        return false;
    }

    if (cpuVirtualAddressSize == 57 && CpuInfo::getInstance().isCpuFlagPresent("la57")) {
        // Always reserve 48 bit window on 57 bit CPU
        if (reservedCpuAddressRangeForNonSvmHeaps.alignedPtr == nullptr) {
            reserveHigh48BitRangeWithMemoryMapsParse(osMemory.get(), reservedCpuAddressRangeForNonSvmHeaps, numRootDevices);

            if (reservedCpuAddressRangeForNonSvmHeaps.alignedPtr == nullptr) {
                reserveLow48BitRangeWithRetry(osMemory.get(), reservedCpuAddressRangeForNonSvmHeaps, numRootDevices);
            }

            if (reservedCpuAddressRangeForNonSvmHeaps.alignedPtr == nullptr) {
                return false;
            }
        }

        gfxBase = castToUint64(reservedCpuAddressRangeForNonSvmHeaps.alignedPtr);
        gfxTop = gfxBase + reservedCpuAddressRangeForNonSvmHeaps.sizeToReserve;
        if (gpuAddressSpace == maxNBitValue(57)) {
            heapInit(HeapIndex::heapSvm, 0ull, maxNBitValue(57 - 1) + 1);
        } else {
            heapInit(HeapIndex::heapSvm, 0ull, maxNBitValue(48) + 1);
        }

        if (gpuAddressSpace == maxNBitValue(57)) {
            uint64_t heapExtendedSize = 4 * systemMemorySize;
            reserve57BitRangeWithMemoryMapsParse(osMemory.get(), reservedCpuAddressRangeForHeapExtended, heapExtendedSize);
            if (reservedCpuAddressRangeForHeapExtended.alignedPtr) {
                heapInit(HeapIndex::heapExtendedHost, castToUint64(reservedCpuAddressRangeForHeapExtended.alignedPtr), heapExtendedSize);
            }
        }
    } else {
        // On 48 bit CPU this range is reserved for OS usage, do not reserve
        gfxBase = maxNBitValue(48 - 1) + 1; // 0x800000000000
        gfxTop = maxNBitValue(48) + 1;      // 0x1000000000000
        heapInit(HeapIndex::heapSvm, 0ull, gfxBase);
    }

    // Init HEAP_EXTENDED only for 57 bit GPU
    if (gpuAddressSpace == maxNBitValue(57)) {
        auto heapExtendedSize = alignDown((maxNBitValue(48) + 1), GfxPartition::heapGranularity);
        heapInit(HeapIndex::heapExtended, maxNBitValue(57 - 1) + 1 + rootDeviceIndex * heapExtendedSize, heapExtendedSize);
    }

    return true;
}

} // namespace NEO
