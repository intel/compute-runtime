/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/sys_calls.h"

#include "drm_neo.h"

namespace NEO {

int Drm::createDrmVirtualMemory(uint32_t &drmVmId) {
    drm_i915_gem_vm_control ctl = {};
    auto ret = SysCalls::ioctl(getFileDescriptor(), DRM_IOCTL_I915_GEM_VM_CREATE, &ctl);
    if (ret == 0) {
        if (ctl.vm_id == 0) {
            // 0 is reserved for invalid/unassigned ppgtt
            return -1;
        }
        drmVmId = ctl.vm_id;
    }
    return ret;
}

bool Drm::isDebugAttachAvailable() {
    return false;
}

int Drm::bindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) {
    return 0;
}

int Drm::unbindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo) {
    return 0;
}

} // namespace NEO
