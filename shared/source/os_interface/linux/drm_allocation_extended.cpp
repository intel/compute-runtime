/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"

namespace NEO {

void DrmAllocation::appendBOs(uint32_t handleId, std::vector<BufferObject *> &bufferObjects) {
    auto bo = this->getBO();
    appendBO(bo, bufferObjects);
}

} // namespace NEO
