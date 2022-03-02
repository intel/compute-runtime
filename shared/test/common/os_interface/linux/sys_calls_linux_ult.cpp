/*
 * Copyright (C) 2020-2022 Intel Corporation
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
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <system_error>

namespace NEO {
namespace SysCalls {
uint32_t closeFuncCalled = 0u;
int closeFuncArgPassed = 0;
int closeFuncRetVal = 0;
int dlOpenFlags = 0;
bool dlOpenCalled = 0;
constexpr int fakeFileDescriptor = 123;
uint32_t vmId = 0;
bool makeFakeDevicePath = false;
bool allowFakeDevicePath = false;
uint32_t ioctlVmCreateCalled = 0u;
int ioctlVmCreateReturned = 0u;
uint32_t vmFlags = 0u;
uint64_t ioctlVmCreateExtensionArg = 0ull;
constexpr unsigned long int invalidIoctl = static_cast<unsigned long int>(-1);
int setErrno = 0;
int fstatFuncRetVal = 0;
uint32_t preadFuncCalled = 0u;
uint32_t mmapFuncCalled = 0u;
uint32_t munmapFuncCalled = 0u;
bool isInvalidAILTest = false;

int (*sysCallsOpen)(const char *pathname, int flags) = nullptr;
ssize_t (*sysCallsPread)(int fd, void *buf, size_t count, off_t offset) = nullptr;
int (*sysCallsReadlink)(const char *path, char *buf, size_t bufsize) = nullptr;
int (*sysCallsIoctl)(int fileDescriptor, unsigned long int request, void *arg) = nullptr;
int (*sysCallsPoll)(struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) = nullptr;
ssize_t (*sysCallsRead)(int fd, void *buf, size_t count) = nullptr;

int close(int fileDescriptor) {
    closeFuncCalled++;
    closeFuncArgPassed = fileDescriptor;
    return closeFuncRetVal;
}

int open(const char *file, int flags) {

    if (sysCallsOpen != nullptr) {
        return sysCallsOpen(file, flags);
    }

    if (strcmp(file, "/dev/dri/by-path/pci-0000:invalid-render") == 0) {
        return 0;
    }
    if (strcmp(file, "./test_files/linux/by-path/pci-0000:00:02.0-render") == 0) {
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

    if (sysCallsIoctl != nullptr) {
        return sysCallsIoctl(fileDescriptor, request, arg);
    }

    if (fileDescriptor == fakeFileDescriptor) {
        if (request == DRM_IOCTL_VERSION) {
            auto pVersion = static_cast<drm_version_t *>(arg);
            snprintf(pVersion->name, pVersion->name_len, "i915");
        }
    }
    if (request == DRM_IOCTL_I915_GEM_VM_CREATE) {
        ioctlVmCreateCalled++;
        auto control = static_cast<drm_i915_gem_vm_control *>(arg);
        ioctlVmCreateExtensionArg = control->extensions;
        control->vm_id = ++vmId;
        vmFlags |= control->flags;
        return ioctlVmCreateReturned;
    }
    if (request == DRM_IOCTL_I915_GEM_VM_DESTROY) {
        auto control = static_cast<drm_i915_gem_vm_control *>(arg);
        vmId--;
        vmFlags = 0;
        return (control->vm_id > 0) ? 0 : -1;
    }
    if (request == invalidIoctl) {
        errno = 0;
        if (setErrno != 0) {
            errno = setErrno;
            setErrno = 0;
        }
        return -1;
    }
    return 0;
}

unsigned int getProcessId() {
    return 0xABCEDF;
}

int access(const char *pathName, int mode) {
    if (allowFakeDevicePath || strcmp(pathName, "/sys/dev/char/226:128") == 0) {
        return 0;
    }
    return -1;
}

int readlink(const char *path, char *buf, size_t bufsize) {

    if (sysCallsReadlink != nullptr) {
        return sysCallsReadlink(path, buf, bufsize);
    }

    if (isInvalidAILTest) {
        return -1;
    }
    if (strcmp(path, "/proc/self/exe") == 0) {
        strcpy_s(buf, sizeof("/proc/self/exe/tests"), "/proc/self/exe/tests");

        return sizeof("/proc/self/exe/tests");
    }

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

int poll(struct pollfd *pollFd, unsigned long int numberOfFds, int timeout) {
    if (sysCallsPoll != nullptr) {
        return sysCallsPoll(pollFd, numberOfFds, timeout);
    }
    return 0;
}

int fstat(int fd, struct stat *buf) {
    return fstatFuncRetVal;
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    if (sysCallsPread != nullptr) {
        return sysCallsPread(fd, buf, count, offset);
    }
    preadFuncCalled++;
    return 0;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    return 0;
}

void *mmap(void *addr, size_t size, int prot, int flags, int fd, off_t off) {
    mmapFuncCalled++;
    return 0;
}

int munmap(void *addr, size_t size) {
    munmapFuncCalled++;
    return 0;
}

ssize_t read(int fd, void *buf, size_t count) {
    if (sysCallsRead != nullptr) {
        return sysCallsRead(fd, buf, count);
    }
    return 0;
}

} // namespace SysCalls
} // namespace NEO
