/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/unified/unified_sharing.h"

#include "core/helpers/string.h"
#include "runtime/context/context.h"
#include "runtime/helpers/timestamp_packet.h"
#include "runtime/sharings/sharing_factory.h"

#include <unordered_map>

namespace NEO {

const uint32_t UnifiedSharingFunctions::sharingId = SharingType::UNIFIED_SHARING;

UnifiedSharing::UnifiedSharing(UnifiedSharingFunctions *sharingFunctions, UnifiedSharingHandleType memoryType)
    : sharingFunctions(sharingFunctions),
      memoryType(memoryType) {
}

int UnifiedSharing::synchronizeHandler(UpdateData &updateData) {
    synchronizeObject(updateData);
    return CL_SUCCESS;
}

void UnifiedSharing::resolveGraphicsAllocationChange(osHandle currentSharedHandle, UpdateData *updateData) {
    updateData->synchronizationStatus = SynchronizeStatus::ACQUIRE_SUCCESFUL;
}

template <>
UnifiedSharingFunctions *Context::getSharing() {
    UNRECOVERABLE_IF(UnifiedSharingFunctions::sharingId >= sharingFunctions.size())
    return reinterpret_cast<UnifiedSharingFunctions *>(sharingFunctions[UnifiedSharingFunctions::sharingId].get());
}

} // namespace NEO
