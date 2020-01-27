/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/unified/unified_sharing.h"

#include "core/helpers/string.h"
#include "core/helpers/timestamp_packet.h"
#include "runtime/context/context.h"
#include "runtime/sharings/sharing_factory.h"

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

void UnifiedSharing::releaseResource(MemObj *memObject) {
}

GraphicsAllocation *UnifiedSharing::createGraphicsAllocation(Context *context, UnifiedSharingMemoryDescription description, GraphicsAllocation::AllocationType allocationType) {
    auto memoryManager = context->getMemoryManager();
    switch (description.type) {
    case UnifiedSharingHandleType::Win32Nt: {
        return memoryManager->createGraphicsAllocationFromNTHandle(description.handle, 0u);
    }
    case UnifiedSharingHandleType::LinuxFd:
    case UnifiedSharingHandleType::Win32Shared: {
        const AllocationProperties properties{0u, false, 0u, allocationType, false};
        return memoryManager->createGraphicsAllocationFromSharedHandle(toOsHandle(description.handle), properties, false);
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
