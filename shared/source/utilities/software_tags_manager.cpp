/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/software_tags_manager.h"

namespace NEO {

void SWTagsManager::initialize(Device &device) {
    UNRECOVERABLE_IF(initialized);
    memoryManager = device.getMemoryManager();
    allocateBXMLHeap(device);
    allocateSWTagHeap(device);
    initialized = true;
}

void SWTagsManager::shutdown() {
    UNRECOVERABLE_IF(!initialized);
    memoryManager->freeGraphicsMemory(bxmlHeap);
    memoryManager->freeGraphicsMemory(tagHeap);
    initialized = false;
}

void SWTagsManager::allocateBXMLHeap(Device &device) {
    SWTags::SWTagBXML tagBXML;
    size_t heapSizeInBytes = sizeof(SWTags::BXMLHeapInfo) + tagBXML.str.size() + 1;

    const AllocationProperties properties{
        device.getRootDeviceIndex(),
        heapSizeInBytes,
        AllocationType::SW_TAG_BUFFER,
        device.getDeviceBitfield()};
    bxmlHeap = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    SWTags::BXMLHeapInfo bxmlHeapInfo(heapSizeInBytes / sizeof(uint32_t));
    MemoryTransferHelper::transferMemoryToAllocation(false, device, bxmlHeap, 0, &bxmlHeapInfo, sizeof(bxmlHeapInfo));
    MemoryTransferHelper::transferMemoryToAllocation(false, device, bxmlHeap, sizeof(bxmlHeapInfo), tagBXML.str.c_str(), heapSizeInBytes - sizeof(bxmlHeapInfo));
}

void SWTagsManager::allocateSWTagHeap(Device &device) {
    const AllocationProperties properties{
        device.getRootDeviceIndex(),
        MAX_TAG_HEAP_SIZE,
        AllocationType::SW_TAG_BUFFER,
        device.getDeviceBitfield()};
    tagHeap = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    SWTags::SWTagHeapInfo tagHeapInfo(MAX_TAG_HEAP_SIZE / sizeof(uint32_t));
    MemoryTransferHelper::transferMemoryToAllocation(false, device, tagHeap, 0, &tagHeapInfo, sizeof(tagHeapInfo));
    currentHeapOffset += sizeof(tagHeapInfo);
}

} // namespace NEO
