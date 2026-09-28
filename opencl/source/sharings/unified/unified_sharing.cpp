/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/sharings/unified/unified_sharing.h"

#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/utilities/stackvec.h"

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

std::unique_ptr<MultiGraphicsAllocation> UnifiedSharing::createMultiGraphicsAllocation(Context *context, UnifiedSharingMemoryDescription description, ImageInfo *imgInfo, AllocationType allocationType, cl_int *errcodeRet, const RootDeviceIndicesContainer &targetRootDeviceIndices) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);
    auto memoryManager = context->getMemoryManager();

    if (description.type != UnifiedSharingHandleType::win32Nt && description.type != UnifiedSharingHandleType::linuxFd && description.type != UnifiedSharingHandleType::win32Shared) {
        errorCode.set(CL_INVALID_MEM_OBJECT);
        return nullptr;
    }

    auto &rootDeviceIndices = (targetRootDeviceIndices.size() > 0) ? targetRootDeviceIndices
                                                                   : context->getRootDeviceIndices();

    StackVec<AllocationProperties, 4> properties;
    for (auto &rootDeviceIndex : rootDeviceIndices) {
        properties.push_back(AllocationProperties{rootDeviceIndex, false, imgInfo, allocationType, context->getDeviceBitfieldForAllocation(rootDeviceIndex)});
    }

    auto multiGraphicsAllocation = memoryManager->createMultiGraphicsAllocationFromSharedHandle(MemoryManager::OsHandleData{description.handle}, properties, true);
    if (!multiGraphicsAllocation) {
        errorCode.set(CL_INVALID_MEM_OBJECT);
        return nullptr;
    }

    return multiGraphicsAllocation;
}

template <>
UnifiedSharingFunctions *Context::getSharing() {
    UNRECOVERABLE_IF(UnifiedSharingFunctions::sharingId >= sharingFunctions.size())
    return reinterpret_cast<UnifiedSharingFunctions *>(sharingFunctions[UnifiedSharingFunctions::sharingId].get());
}

} // namespace NEO
