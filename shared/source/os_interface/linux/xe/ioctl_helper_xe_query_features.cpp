/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

#include "xe_drm.h"

namespace NEO {
void IoctlHelperXe::querySupportedFeatures() {
    auto checkVmCreateFlagsSupport = [&](uint32_t flags) -> bool {
        struct drm_xe_vm_create vmCreate = {};
        vmCreate.flags = flags;

        auto ret = IoctlHelper::ioctl(DrmIoctl::gemVmCreate, &vmCreate);
        if (ret == 0) {
            struct drm_xe_vm_destroy vmDestroy = {};
            vmDestroy.vm_id = vmCreate.vm_id;
            ret = IoctlHelper::ioctl(DrmIoctl::gemVmDestroy, &vmDestroy);
            DEBUG_BREAK_IF(ret != 0);
            return true;
        }
        return false;
    };
    supportedFeatures.flags.pageFault = checkVmCreateFlagsSupport(DRM_XE_VM_CREATE_FLAG_LR_MODE | DRM_XE_VM_CREATE_FLAG_FAULT_MODE);
};

uint64_t IoctlHelperXe::getAdditionalFlagsForVmBind(bool bindImmediate, bool readOnlyResource) {
    return 0;
}
} // namespace NEO