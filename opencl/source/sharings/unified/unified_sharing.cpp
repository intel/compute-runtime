/*
 * Copyright (C) 2019-2025 Intel Corporation
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

std::unique_ptr<MultiGraphicsAllocation> UnifiedSharing::createMultiGraphicsAllocation(Context *context, UnifiedSharingMemoryDescription description, ImageInfo *imgInfo, AllocationType allocationType, cl_int *errcodeRet) {
    ErrorCodeHelper errorCode(errcodeRet, CL_SUCCESS);
    auto memoryManager = context->getMemoryManager();

    if (description.type != UnifiedSharingHandleType::win32Nt && description.type != UnifiedSharingHandleType::linuxFd && description.type != UnifiedSharingHandleType::win32Shared) {
        errorCode.set(CL_INVALID_MEM_OBJECT);
        return nullptr;
    }

    auto pRootDeviceIndices = &context->getRootDeviceIndices();
    auto multiGraphicsAllocation = std::make_unique<MultiGraphicsAllocation>(context->getMaxRootDeviceIndex());
    MemoryManager::OsHandleData osHandleData{description.handle};

    for (auto &rootDeviceIndex : *pRootDeviceIndices) {
        const AllocationProperties properties{rootDeviceIndex,
                                              false, // allocateMemory
                                              imgInfo,
                                              allocationType,
                                              context->getDeviceBitfieldForAllocation(rootDeviceIndex)};

        auto graphicsAllocation = memoryManager->createGraphicsAllocationFromSharedHandle(osHandleData, properties, false, false, true, nullptr);
        if (!graphicsAllocation) {
            errorCode.set(CL_INVALID_MEM_OBJECT);
            return nullptr;
        }

        multiGraphicsAllocation->addAllocation(graphicsAllocation);
    }

    return multiGraphicsAllocation;
}

template <>
UnifiedSharingFunctions *Context::getSharing() {
    UNRECOVERABLE_IF(UnifiedSharingFunctions::sharingId >= sharingFunctions.size())
    return reinterpret_cast<UnifiedSharingFunctions *>(sharingFunctions[UnifiedSharingFunctions::sharingId].get());
}

} // namespace NEO
