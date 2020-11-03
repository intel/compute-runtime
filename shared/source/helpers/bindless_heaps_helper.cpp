/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bindless_heaps_helper.h"

#include "shared/source/helpers/string.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {

constexpr size_t globalSshAllocationSize = 4 * MemoryConstants::pageSize64k;
using BindlesHeapType = BindlessHeapsHelper::BindlesHeapType;

BindlessHeapsHelper::BindlessHeapsHelper(MemoryManager *memManager, bool isMultiOsContextCapable, const uint32_t rootDeviceIndex) : memManager(memManager), isMultiOsContextCapable(isMultiOsContextCapable), rootDeviceIndex(rootDeviceIndex) {
    for (auto heapType = 0; heapType < BindlesHeapType::NUM_HEAP_TYPES; heapType++) {
        auto allocInFrontWindow = heapType != BindlesHeapType::GLOBAL_DSH;
        auto heapAllocation = getHeapAllocation(MemoryConstants::pageSize64k, MemoryConstants::pageSize64k, allocInFrontWindow);
        UNRECOVERABLE_IF(heapAllocation == nullptr);
        ssHeapsAllocations.push_back(heapAllocation);
        surfaceStateHeaps[heapType] = std::make_unique<IndirectHeap>(heapAllocation, false);
    }

    borderColorStates = getHeapAllocation(MemoryConstants::pageSize, MemoryConstants::pageSize, true);
    UNRECOVERABLE_IF(borderColorStates == nullptr);
    float borderColorDefault[4] = {0, 0, 0, 0};
    memcpy_s(borderColorStates->getUnderlyingBuffer(), sizeof(borderColorDefault), borderColorDefault, sizeof(borderColorDefault));
    float borderColorAlpha[4] = {0, 0, 0, 1.0};
    memcpy_s(ptrOffset(borderColorStates->getUnderlyingBuffer(), sizeof(borderColorDefault)), sizeof(borderColorAlpha), borderColorAlpha, sizeof(borderColorAlpha));
}

BindlessHeapsHelper::~BindlessHeapsHelper() {
    for (auto *allocation : ssHeapsAllocations) {
        memManager->freeGraphicsMemory(allocation);
    }
    memManager->freeGraphicsMemory(borderColorStates);
    ssHeapsAllocations.clear();
}

GraphicsAllocation *BindlessHeapsHelper::getHeapAllocation(size_t heapSize, size_t alignment, bool allocInFrontWindow) {
    auto allocationType = GraphicsAllocation::AllocationType::LINEAR_STREAM;
    NEO::AllocationProperties properties{rootDeviceIndex, true, heapSize, allocationType, isMultiOsContextCapable, false};
    properties.flags.use32BitFrontWindow = allocInFrontWindow;
    properties.alignment = alignment;

    return this->memManager->allocateGraphicsMemoryWithProperties(properties);
}

SurfaceStateInHeapInfo BindlessHeapsHelper::allocateSSInHeap(size_t ssSize, GraphicsAllocation *surfaceAllocation, BindlesHeapType heapType) {
    auto heap = surfaceStateHeaps[heapType].get();
    if (heapType == BindlesHeapType::GLOBAL_SSH) {
        auto ssAllocatedInfo = surfaceStateInHeapAllocationMap.find(surfaceAllocation);
        if (ssAllocatedInfo != surfaceStateInHeapAllocationMap.end()) {
            return *ssAllocatedInfo->second.get();
        }
    }
    void *ptrInHeap = getSpaceInHeap(ssSize, heapType);
    auto bindlessOffset = heap->getGraphicsAllocation()->getGpuAddress() - heap->getGraphicsAllocation()->getGpuBaseAddress() + heap->getUsed() - ssSize;
    SurfaceStateInHeapInfo bindlesInfo;
    if (heapType == BindlesHeapType::GLOBAL_SSH) {
        std::pair<GraphicsAllocation *, std::unique_ptr<SurfaceStateInHeapInfo>> pair(surfaceAllocation, std::make_unique<SurfaceStateInHeapInfo>(SurfaceStateInHeapInfo{heap->getGraphicsAllocation(), bindlessOffset, ptrInHeap}));
        bindlesInfo = *pair.second;
        surfaceStateInHeapAllocationMap.insert(std::move(pair));
    } else {
        bindlesInfo = SurfaceStateInHeapInfo{heap->getGraphicsAllocation(), bindlessOffset, ptrInHeap};
    }
    return bindlesInfo;
}

void *BindlessHeapsHelper::getSpaceInHeap(size_t ssSize, BindlesHeapType heapType) {
    auto heap = surfaceStateHeaps[heapType].get();
    if (heap->getAvailableSpace() < ssSize) {
        growHeap(heapType);
    }
    return heap->getSpace(ssSize);
}

uint64_t BindlessHeapsHelper::getGlobalHeapsBase() {
    return surfaceStateHeaps[BindlesHeapType::GLOBAL_SSH]->getGraphicsAllocation()->getGpuBaseAddress();
}

uint32_t BindlessHeapsHelper::getDefaultBorderColorOffset() {
    return static_cast<uint32_t>(borderColorStates->getGpuAddress() - borderColorStates->getGpuBaseAddress());
}
uint32_t BindlessHeapsHelper::getAlphaBorderColorOffset() {
    return getDefaultBorderColorOffset() + 4 * sizeof(float);
}

void BindlessHeapsHelper::growHeap(BindlesHeapType heapType) {
    auto heap = surfaceStateHeaps[heapType].get();
    auto allocInFrontWindow = heapType != BindlesHeapType::GLOBAL_DSH;
    auto newAlloc = getHeapAllocation(globalSshAllocationSize, MemoryConstants::pageSize64k, allocInFrontWindow);
    UNRECOVERABLE_IF(newAlloc == nullptr);
    ssHeapsAllocations.push_back(newAlloc);
    heap->replaceGraphicsAllocation(newAlloc);
    heap->replaceBuffer(newAlloc->getUnderlyingBuffer(),
                        newAlloc->getUnderlyingBufferSize());
}

} // namespace NEO