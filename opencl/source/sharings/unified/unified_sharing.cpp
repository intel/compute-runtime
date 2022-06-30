/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/unified/unified_sharing.h"

#include "shared/source/helpers/string.h"
#include "shared/source/helpers/timestamp_packet.h"

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
    auto memoryManager = context->getMemoryManager();
    switch (description.type) {
    case UnifiedSharingHandleType::Win32Nt: {
        return memoryManager->createGraphicsAllocationFromNTHandle(description.handle, context->getDevice(0)->getRootDeviceIndex(), allocationType);
    }
    case UnifiedSharingHandleType::LinuxFd:
    case UnifiedSharingHandleType::Win32Shared: {
        const AllocationProperties properties{context->getDevice(0)->getRootDeviceIndex(),
                                              false, // allocateMemory
                                              0u,    // size
                                              allocationType,
                                              false, // isMultiStorageAllocation
                                              context->getDeviceBitfieldForAllocation(context->getDevice(0)->getRootDeviceIndex())};
        return memoryManager->createGraphicsAllocationFromSharedHandle(toOsHandle(description.handle), properties, false, false);
    }
    default:
        return nullptr;
    }
}

template <>
UnifiedSharingFunctions *Context::getSharing() {
    UNRECOVERABLE_IF(UnifiedSharingFunctions::sharingId >= sharingFunctions.size())
    return reinterpret_cast<UnifiedSharingFunctions *>(sharingFunctions[UnifiedSharingFunctions::sharingId].get());
}

} // namespace NEO
