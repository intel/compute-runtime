/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {

int DrmAllocation::bindBOs(OsContext *osContext, uint32_t vmHandleId, std::vector<BufferObject *> *bufferObjects, bool bind) {
    auto bo = this->getBO();
    return bindBO(bo, osContext, vmHandleId, bufferObjects, bind);
}

bool DrmAllocation::shouldAllocationPageFault(const Drm *drm) {
    return false;
}

} // namespace NEO
