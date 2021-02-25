/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "drm/i915_drm.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <system_error>

namespace NEO {
namespace SysCalls {
uint32_t closeFuncCalled = 0u;
int closeFuncArgPassed = 0;
int dlOpenFlags = 0;
bool dlOpenCalled = 0;
constexpr int fakeFileDescriptor = 123;
uint32_t vmId = 0;
bool makeFakeDevicePath = false;
bool allowFakeDevicePath = false;

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

void *dlopen(const char *filename, int flag) {
    dlOpenFlags = flag;
    dlOpenCalled = true;
    return ::dlopen(filename, flag);
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

int access(const char *pathName, int mode) {
    if (allowFakeDevicePath || strcmp(pathName, "/sys/dev/char/226:128") == 0) {
        return 0;
    }
    return -1;
}

int readlink(const char *path, char *buf, size_t bufsize) {
    if (strcmp(path, "/sys/dev/char/226:128") != 0) {
        return -1;
    }

    constexpr size_t sizeofPath = sizeof("../../devices/pci0000:4a/0000:4a:02.0/0000:4b:00.0/0000:4c:01.0/0000:00:02.0/drm/renderD128");

    strcpy_s(buf, sizeofPath, "../../devices/pci0000:4a/0000:4a:02.0/0000:4b:00.0/0000:4c:01.0/0000:00:02.0/drm/renderD128");
    return sizeofPath;
}

int getDevicePath(int deviceFd, char *buf, size_t &bufSize) {
    if (deviceFd <= 0) {
        return -1;
    }
    constexpr size_t sizeofPath = sizeof("/sys/dev/char/226:128");

    makeFakeDevicePath ? strcpy_s(buf, sizeofPath, "/sys/dev/char/xyzwerq") : strcpy_s(buf, sizeofPath, "/sys/dev/char/226:128");
    bufSize = sizeofPath;

    return 0;
}
} // namespace SysCalls
} // namespace NEO
