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

BindlessHeapsHelper::BindlessHeapsHelper(MemoryManager *memManager, bool isMultiOsContextCapable, const uint32_t rootDeviceIndex) : memManager(memManager), isMultiOsContextCapable(isMultiOsContextCapable), rootDeviceIndex(rootDeviceIndex) {
    auto specialHeapAllocation = getHeapAllocation(MemoryConstants::pageSize64k, MemoryConstants::pageSize64k);
    UNRECOVERABLE_IF(specialHeapAllocation == nullptr);
    ssHeapsAllocations.push_back(specialHeapAllocation);
    specialSsh = std::make_unique<IndirectHeap>(specialHeapAllocation, false);

    auto globalSshAllocation = getHeapAllocation(globalSshAllocationSize, MemoryConstants::pageSize64k);
    UNRECOVERABLE_IF(globalSshAllocation == nullptr);
    ssHeapsAllocations.push_back(globalSshAllocation);
    globalSsh = std::make_unique<IndirectHeap>(globalSshAllocation, false);

    borderColorStates = getHeapAllocation(MemoryConstants::pageSize, MemoryConstants::pageSize);
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

GraphicsAllocation *BindlessHeapsHelper::getHeapAllocation(size_t heapSize, size_t alignment) {
    auto allocationType = GraphicsAllocation::AllocationType::LINEAR_STREAM;
    NEO::AllocationProperties properties{rootDeviceIndex, true, heapSize, allocationType, isMultiOsContextCapable, false};
    properties.flags.use32BitFrontWindow = true;
    properties.alignment = alignment;

    return this->memManager->allocateGraphicsMemoryWithProperties(properties);
}

SurfaceStateInHeapInfo *BindlessHeapsHelper::allocateSSInHeap(size_t ssSize, void *ssPtr, GraphicsAllocation *surfaceAllocation) {
    auto ssAllocatedInfo = surfaceStateInHeapAllocationMap.find(surfaceAllocation);
    if (ssAllocatedInfo != surfaceStateInHeapAllocationMap.end()) {
        return ssAllocatedInfo->second.get();
    }
    void *ptrInHeap = getSpaceInSsh(ssSize);
    memcpy_s(ptrInHeap, ssSize, ssPtr, ssSize);
    auto bindlessOffset = globalSsh->getGraphicsAllocation()->getGpuAddress() - globalSsh->getGraphicsAllocation()->getGpuBaseAddress() + globalSsh->getUsed() - ssSize;
    std::pair<GraphicsAllocation *, std::unique_ptr<SurfaceStateInHeapInfo>> pair(surfaceAllocation, std::make_unique<SurfaceStateInHeapInfo>(SurfaceStateInHeapInfo{globalSsh->getGraphicsAllocation(), bindlessOffset}));
    auto bindlesInfo = pair.second.get();
    surfaceStateInHeapAllocationMap.insert(std::move(pair));
    return bindlesInfo;
}

void *BindlessHeapsHelper::getSpaceInSsh(size_t ssSize) {
    if (globalSsh->getAvailableSpace() < ssSize) {
        growGlobalSSh();
    }
    return globalSsh->getSpace(ssSize);
}

uint64_t BindlessHeapsHelper::getGlobalSshBase() {
    return globalSsh->getGraphicsAllocation()->getGpuBaseAddress();
}

uint32_t BindlessHeapsHelper::getDefaultBorderColorOffset() {
    return static_cast<uint32_t>(borderColorStates->getGpuAddress() - borderColorStates->getGpuBaseAddress());
}
uint32_t BindlessHeapsHelper::getAlphaBorderColorOffset() {
    return getDefaultBorderColorOffset() + 4 * sizeof(float);
}

void BindlessHeapsHelper::growGlobalSSh() {
    auto newAlloc = getHeapAllocation(globalSshAllocationSize, MemoryConstants::pageSize64k);
    UNRECOVERABLE_IF(newAlloc == nullptr);
    ssHeapsAllocations.push_back(newAlloc);
    globalSsh->replaceGraphicsAllocation(newAlloc);
    globalSsh->replaceBuffer(newAlloc->getUnderlyingBuffer(),
                             newAlloc->getUnderlyingBufferSize());
}

} // namespace NEO