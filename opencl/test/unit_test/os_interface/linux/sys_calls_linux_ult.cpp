/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/sys_calls.h"

#include "drm/i915_drm.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/ioctl.h>

namespace NEO {
namespace SysCalls {
uint32_t closeFuncCalled = 0u;
int closeFuncArgPassed = 0;
constexpr int fakeFileDescriptor = 123;
uint32_t vmId = 0;

int close(int fileDescriptor) {
    closeFuncCalled++;
    closeFuncArgPassed = fileDescriptor;
    return 0;
}

int open(const char *file, int flags) {
    if (strcmp(file, "/dev/dri/by-path/pci-0000:invalid-render") == 0) {
        return 0;
    }
    if (strcmp(file, "./test_files/linux/by-path/pci-0000-device-render") == 0) {
        return fakeFileDescriptor;
    }
    return 0;
}
int ioctl(int fileDescriptor, unsigned long int request, void *arg) {
    if (fileDescriptor == fakeFileDescriptor) {
        if (request == DRM_IOCTL_VERSION) {
            auto pVersion = static_cast<drm_version_t *>(arg);
            snprintf(pVersion->name, pVersion->name_len, "i915");
        }
    }
    if (request == DRM_IOCTL_I915_GEM_VM_CREATE) {
        auto control = static_cast<drm_i915_gem_vm_control *>(arg);
        control->vm_id = ++vmId;
        return 0;
    }
    if (request == DRM_IOCTL_I915_GEM_VM_DESTROY) {
        auto control = static_cast<drm_i915_gem_vm_control *>(arg);
        vmId--;
        return (control->vm_id > 0) ? 0 : -1;
    }
    return 0;
}
} // namespace SysCalls
} // namespace NEO
