/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_allocation.h"
#include "runtime/os_interface/linux/drm_command_stream.h"

namespace NEO {
template <typename GfxFamily>
void DrmCommandStreamReceiver<GfxFamily>::makeResidentBufferObjects(const DrmAllocation *drmAllocation) {
    auto bo = drmAllocation->getBO();
    makeResident(bo);
}
} // namespace NEO
