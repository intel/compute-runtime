/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bindless_heaps_helper.h"

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/heap_allocator.h"

namespace NEO {

constexpr size_t globalSshAllocationSize = 4 * MemoryConstants::pageSize64k;
constexpr size_t borderColorAlphaOffset = alignUp(4 * sizeof(float), MemoryConstants::cacheLineSize);

constexpr HeapIndex heapIndexForPoolReservedRange = HeapIndex::heapStandard;
constexpr size_t reservedRangeSize = static_cast<size_t>(4 * MemoryConstants::gigaByte);
constexpr size_t heapFrontWindowSize = GfxPartition::externalFrontWindowPoolSize;
constexpr size_t heapRegularSize = reservedRangeSize - heapFrontWindowSize;

/*
 *       __________________________________ STANDARD __________________________________
 *      /                                                                              \
 *     /             ____________________ Reserved 4GB _____________________            \
 *    /             /                                                       \            \
 *   /             /                                                         \            \
 *   |_____________|_________________|_______________________________________|____________|
 *   |             |                 |                                       |            |
 *                   heapFrontWindow                heapRegular
 *                 ^
 *          reservedRangeBase
 */

using BindlesHeapType = BindlessHeapsHelper::BindlesHeapType;

BindlessHeapsHelper::BindlessHeapsHelper(Device *rootDevice, bool isMultiOsContextCapable) : rootDevice(rootDevice),
                                                                                             memManager(rootDevice->getMemoryManager()),
                                                                                             deviceBitfield(rootDevice->getDeviceBitfield()),
                                                                                             surfaceStateSize(rootDevice->getRootDeviceEnvironment().getHelper<GfxCoreHelper>().getRenderSurfaceStateSize()),
                                                                                             rootDeviceIndex(rootDevice->getRootDeviceIndex()),
                                                                                             isMultiOsContextCapable(isMultiOsContextCapable) {

    for (auto heapType = 0; heapType < BindlesHeapType::numHeapTypes; heapType++) {
        auto size = MemoryConstants::pageSize64k;

        if (heapType == BindlesHeapType::specialSsh) {
            if (isReservedMemoryModeAvailable() &&
                !tryReservingMemoryForSpecialSsh(size, MemoryConstants::pageSize64k)) {
                useReservedMemory = initializeReservedMemory();
            }
        }

        auto heapAllocation = getHeapAllocation(size, MemoryConstants::pageSize64k, heapType == BindlesHeapType::specialSsh);
        UNRECOVERABLE_IF(heapAllocation == nullptr);
        ssHeapsAllocations.push_back(heapAllocation);
        surfaceStateHeaps[heapType] = std::make_unique<IndirectHeap>(heapAllocation, true);
    }

    borderColorStates = getHeapAllocation(MemoryConstants::pageSize, MemoryConstants::pageSize, false);
    UNRECOVERABLE_IF(borderColorStates == nullptr);
    float borderColorDefault[4] = {0, 0, 0, 0};
    memcpy_s(borderColorStates->getUnderlyingBuffer(), sizeof(borderColorDefault), borderColorDefault, sizeof(borderColorDefault));
    float borderColorAlpha[4] = {0, 0, 0, 1.0};
    memcpy_s(ptrOffset(borderColorStates->getUnderlyingBuffer(), borderColorAlphaOffset), sizeof(borderColorAlpha), borderColorAlpha, sizeof(borderColorDefault));

    auto &hwInfo = *rootDevice->getRootDeviceEnvironment().getHardwareInfo();
    this->heaplessEnabled = rootDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>().isHeaplessModeEnabled(hwInfo);
}

std::optional<AddressRange> BindlessHeapsHelper::reserveMemoryRange(size_t size, size_t alignment, HeapIndex heapIndex) {
    RootDeviceIndicesContainer rootDeviceIndices;
    rootDeviceIndices.pushUnique(rootDeviceIndex);

    uint32_t reservedOnRootDevice = 0;
    auto reservedRange = memManager->reserveGpuAddressOnHeap(
        0ull, size, rootDeviceIndices, &reservedOnRootDevice, heapIndex, alignment);

    if (reservedRange.address == 0u) {
        return std::nullopt;
    }

    reservedRanges.push_back({reservedRange.address, reservedRange.size});

    return reservedRange;
}

bool BindlessHeapsHelper::tryReservingMemoryForSpecialSsh(const size_t size, size_t alignment) {
    auto heapIndex = memManager->isLocalMemorySupported(rootDeviceIndex) ? HeapIndex::heapExternalDeviceFrontWindow : HeapIndex::heapExternalFrontWindow;
    auto reservedRange = reserveMemoryRange(size, alignment, heapIndex);
    return reservedRange.has_value();
}

bool BindlessHeapsHelper::initializeReservedMemory() {
    if (reservedMemoryInitialized) {
        return true;
    }

    auto reservedRangeOpt = reserveMemoryRange(reservedRangeSize, MemoryConstants::pageSize64k, heapIndexForPoolReservedRange);
    if (!reservedRangeOpt.has_value()) {
        return false;
    }

    DEBUG_BREAK_IF((reservedRangeOpt.value().address % MemoryConstants::pageSize64k) != 0);

    auto reservedRange = reservedRangeOpt.value();
    reservedRangeBase = rootDevice->getRootDeviceEnvironmentRef().getGmmHelper()->decanonize(reservedRange.address);

    heapFrontWindow = std::make_unique<HeapAllocator>(reservedRangeBase, heapFrontWindowSize, MemoryConstants::pageSize64k, 0);
    heapRegular = std::make_unique<HeapAllocator>(reservedRangeBase + heapFrontWindowSize, heapRegularSize, MemoryConstants::pageSize64k, 0);

    memManager->addCustomHeapAllocatorConfig(AllocationType::linearStream, true, {heapFrontWindow.get(), reservedRangeBase});
    memManager->addCustomHeapAllocatorConfig(AllocationType::linearStream, false, {heapRegular.get(), reservedRangeBase});

    reservedMemoryInitialized = true;
    return true;
}

BindlessHeapsHelper::~BindlessHeapsHelper() {
    for (auto *allocation : ssHeapsAllocations) {
        memManager->freeGraphicsMemory(allocation);
    }
    memManager->freeGraphicsMemory(borderColorStates);
    ssHeapsAllocations.clear();

    for (const auto &range : reservedRanges) {
        memManager->freeGpuAddress(range, rootDeviceIndex);
    }
    reservedRanges.clear();

    if (reservedMemoryInitialized) {
        memManager->removeCustomHeapAllocatorConfig(AllocationType::linearStream, true);
        memManager->removeCustomHeapAllocatorConfig(AllocationType::linearStream, false);
    }
}

GraphicsAllocation *BindlessHeapsHelper::getHeapAllocation(size_t heapSize, size_t alignment, bool allocInFrontWindow) {
    auto allocationType = AllocationType::linearStream;
    NEO::AllocationProperties properties{rootDeviceIndex, true, heapSize, allocationType, isMultiOsContextCapable, deviceBitfield};
    properties.flags.use32BitFrontWindow = allocInFrontWindow;
    properties.alignment = alignment;

    GraphicsAllocation *allocation = memManager->allocateGraphicsMemoryWithProperties(properties);
    MemoryOperationsHandler *memoryOperationsIface = rootDevice->getRootDeviceEnvironmentRef().memoryOperationsInterface.get();
    auto result = memoryOperationsIface->makeResident(rootDevice, ArrayRef<NEO::GraphicsAllocation *>(&allocation, 1), false, false);
    if (result != NEO::MemoryOperationsStatus::success) {
        memManager->freeGraphicsMemory(allocation);
        return nullptr;
    }
    return allocation;
}

bool BindlessHeapsHelper::isReservedMemoryModeAvailable() {
    auto osInterface = rootDevice->getRootDeviceEnvironment().osInterface.get();
    if (!osInterface) {
        return false;
    }
    return osInterface->getDriverModel()->getDriverModelType() == NEO::DriverModelType::wddm;
}

void BindlessHeapsHelper::clearStateDirtyForContext(uint32_t osContextId) {
    std::lock_guard<std::mutex> autolock(this->mtx);

    uint32_t contextIdShifted = osContextId - memManager->getFirstContextIdForRootDevice(rootDeviceIndex);
    DEBUG_BREAK_IF(contextIdShifted >= stateCacheDirtyForContext.size());

    stateCacheDirtyForContext.reset(contextIdShifted);
}

bool BindlessHeapsHelper::getStateDirtyForContext(uint32_t osContextId) {
    std::lock_guard<std::mutex> autolock(this->mtx);

    uint32_t contextIdShifted = osContextId - memManager->getFirstContextIdForRootDevice(rootDeviceIndex);
    DEBUG_BREAK_IF(contextIdShifted >= stateCacheDirtyForContext.size());

    return stateCacheDirtyForContext.test(contextIdShifted);
}

SurfaceStateInHeapInfo BindlessHeapsHelper::allocateSSInHeap(size_t ssSize, GraphicsAllocation *surfaceAllocation, BindlesHeapType heapType) {
    auto heap = surfaceStateHeaps[heapType].get();

    std::lock_guard<std::mutex> autolock(this->mtx);
    if (heapType == BindlesHeapType::globalSsh) {

        if (!allocateFromReusePool) {
            if ((surfaceStateInHeapVectorReuse[releasePoolIndex][0].size() + surfaceStateInHeapVectorReuse[releasePoolIndex][1].size()) > reuseSlotCountThreshold) {

                // invalidate all contexts
                stateCacheDirtyForContext.set();
                allocateFromReusePool = true;
                allocatePoolIndex = releasePoolIndex;
                releasePoolIndex = allocatePoolIndex == 0 ? 1 : 0;
            }
        }

        if (allocateFromReusePool) {
            int index = getReusedSshVectorIndex(ssSize);

            if (surfaceStateInHeapVectorReuse[allocatePoolIndex][index].size()) {
                SurfaceStateInHeapInfo surfaceStateFromVector = surfaceStateInHeapVectorReuse[allocatePoolIndex][index].back();
                surfaceStateInHeapVectorReuse[allocatePoolIndex][index].pop_back();

                if (surfaceStateInHeapVectorReuse[allocatePoolIndex][index].empty()) {
                    allocateFromReusePool = false;

                    // copy remaining slots from allocate pool to release pool
                    int otherSizeIndex = index == 0 ? 1 : 0;
                    surfaceStateInHeapVectorReuse[releasePoolIndex][otherSizeIndex].insert(surfaceStateInHeapVectorReuse[releasePoolIndex][otherSizeIndex].end(),
                                                                                           surfaceStateInHeapVectorReuse[allocatePoolIndex][otherSizeIndex].begin(),
                                                                                           surfaceStateInHeapVectorReuse[allocatePoolIndex][otherSizeIndex].end());

                    surfaceStateInHeapVectorReuse[allocatePoolIndex][otherSizeIndex].clear();
                }

                return surfaceStateFromVector;
            }
        }
    }

    void *ptrInHeap = getSpaceInHeap(ssSize, heapType);
    SurfaceStateInHeapInfo bindlesInfo = {nullptr, 0, nullptr};

    if (ptrInHeap) {
        memset(ptrInHeap, 0, ssSize);
        auto bindlessOffset = heap->getGraphicsAllocation()->getGpuAddress() - heap->getGraphicsAllocation()->getGpuBaseAddress() + heap->getUsed() - ssSize;

        if (this->heaplessEnabled) {
            bindlessOffset += heap->getGraphicsAllocation()->getGpuBaseAddress();
        }

        bindlesInfo = SurfaceStateInHeapInfo{heap->getGraphicsAllocation(), bindlessOffset, ptrInHeap, ssSize};
    }

    return bindlesInfo;
}

void *BindlessHeapsHelper::getSpaceInHeap(size_t ssSize, BindlesHeapType heapType) {
    auto heap = surfaceStateHeaps[heapType].get();
    if (heap->getAvailableSpace() < ssSize) {
        if (!growHeap(heapType)) {
            return nullptr;
        }
    }
    return heap->getSpace(ssSize);
}

uint64_t BindlessHeapsHelper::getGlobalHeapsBase() {
    return surfaceStateHeaps[BindlesHeapType::globalSsh]->getGraphicsAllocation()->getGpuBaseAddress();
}

uint32_t BindlessHeapsHelper::getDefaultBorderColorOffset() {
    return static_cast<uint32_t>(borderColorStates->getGpuAddress() - borderColorStates->getGpuBaseAddress());
}
uint32_t BindlessHeapsHelper::getAlphaBorderColorOffset() {
    return getDefaultBorderColorOffset() + borderColorAlphaOffset;
}

IndirectHeap *BindlessHeapsHelper::getHeap(BindlesHeapType heapType) {
    return surfaceStateHeaps[heapType].get();
}

bool BindlessHeapsHelper::growHeap(BindlesHeapType heapType) {
    auto heap = surfaceStateHeaps[heapType].get();
    auto allocInFrontWindow = false;
    auto newAlloc = getHeapAllocation(globalSshAllocationSize, MemoryConstants::pageSize64k, allocInFrontWindow);
    DEBUG_BREAK_IF(newAlloc == nullptr);
    if (newAlloc == nullptr) {
        return false;
    }
    ssHeapsAllocations.push_back(newAlloc);
    heap->replaceGraphicsAllocation(newAlloc);
    heap->replaceBuffer(newAlloc->getUnderlyingBuffer(),
                        newAlloc->getUnderlyingBufferSize());
    return true;
}

void BindlessHeapsHelper::releaseSSToReusePool(const SurfaceStateInHeapInfo &surfStateInfo) {
    if (surfStateInfo.heapAllocation != nullptr) {
        std::lock_guard<std::mutex> autolock(this->mtx);
        int index = getReusedSshVectorIndex(surfStateInfo.ssSize);
        surfaceStateInHeapVectorReuse[releasePoolIndex][index].push_back(surfStateInfo);
    }

    return;
}

} // namespace NEO