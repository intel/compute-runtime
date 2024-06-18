/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/unified/unified_sharing.h"

#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/allocation_properties.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/context/context.h"
#include "opencl/source/sharings/sharing_factory.h"

#include <unordered_map>

namespace NEO {

const uint32_t UnifiedSharingFunctions::sharingId = SharingType::UNIFIED_SHARING;

UnifiedSharing::UnifiedSharing(UnifiedSharingFunctions *sharingFunctions, UnifiedSharingHandleType memoryType)
    : sharingFunctions(sharingFunctions),
      memoryType(memoryType) {
}

void UnifiedSharing::synchronizeObject(UpdateData &updateData) {
    updateData.synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
}

void UnifiedSharing::releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) {
}

GraphicsAllocation *UnifiedSharing::createGraphicsAllocation(Context *context, UnifiedSharingMemoryDescription description, AllocationType allocationType) {
    return createGraphicsAllocation(context, description, nullptr, allocationType);
}

GraphicsAllocation *UnifiedSharing::createGraphicsAllocation(Context *context, UnifiedSharingMemoryDescription description, ImageInfo *imgInfo, AllocationType allocationType) {
    auto memoryManager = context->getMemoryManager();

    if (description.type != UnifiedSharingHandleType::win32Nt && description.type != UnifiedSharingHandleType::linuxFd && description.type != UnifiedSharingHandleType::win32Shared)
        return nullptr;

    const AllocationProperties properties{context->getDevice(0)->getRootDeviceIndex(),
                                          false, // allocateMemory
                                          imgInfo,
                                          allocationType,
                                          context->getDeviceBitfieldForAllocation(context->getDevice(0)->getRootDeviceIndex())};
    MemoryManager::OsHandleData osHandleData{description.handle};
    return memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
}

template <>
UnifiedSharingFunctions *Context::getSharing() {
    UNRECOVERABLE_IF(UnifiedSharingFunctions::sharingId >= sharingFunctions.size())
    return reinterpret_cast<UnifiedSharingFunctions *>(sharingFunctions[UnifiedSharingFunctions::sharingId].get());
}

} // namespace NEO
