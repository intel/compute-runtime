/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"

namespace NEO {

void DrmAllocation::bindBOs(uint32_t handleId, uint32_t drmContextId, std::vector<BufferObject *> *bufferObjects, bool bind) {
    auto bo = this->getBO();
    bindBO(bo, drmContextId, bufferObjects, bind);
}

} // namespace NEO
