/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/gfx_partition.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/file_descriptor.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "drm.h"

#include <fcntl.h>
#include <sstream>

namespace NEO {

std::optional<std::function<std::unique_ptr<IoctlHelper>(Drm &drm)>> ioctlHelperFactory[IGFX_MAX_PRODUCT] = {};
void IoctlHelper::setExternalContext(ExternalCtx *ctx) {
    externalCtx = ctx;
}

int IoctlHelper::ioctl(DrmIoctl request, void *arg) {
    if (externalCtx) {
        return externalCtx->ioctl(externalCtx->handle, drm.getFileDescriptor(), getIoctlRequestValue(request), arg, false);
    }
    return drm.ioctl(request, arg);
}

int IoctlHelper::ioctl(int fd, DrmIoctl request, void *arg) {
    return NEO::SysCalls::ioctl(fd, getIoctlRequestValue(request), arg);
}

void IoctlHelper::setupIpVersion() {
    auto &rootDeviceEnvironment = drm.getRootDeviceEnvironment();
    auto &hwInfo = *rootDeviceEnvironment.getMutableHardwareInfo();
    auto &compilerProductHelper = rootDeviceEnvironment.getHelper<CompilerProductHelper>();
    hwInfo.ipVersion.value = compilerProductHelper.getHwIpVersion(hwInfo);
}

uint32_t IoctlHelper::getFlagsForPrimeHandleToFd() const {
    return DRM_CLOEXEC | DRM_RDWR;
}

void IoctlHelper::writeCcsMode(const std::string &gtFile, uint32_t ccsMode,
                               std::vector<std::tuple<std::string, uint32_t>> &deviceCcsModeVec) {

    std::string ccsFile = gtFile + "/ccs_mode";
    auto fd = FileDescriptor(ccsFile.c_str(), O_RDWR);
    if (fd < 0) {
        if ((errno == -EACCES) || (errno == -EPERM)) {
            fprintf(stderr, "No read and write permissions for %s, System administrator needs to grant permissions to allow modification of this file from user space\n", ccsFile.c_str());
            fprintf(stdout, "No read and write permissions for %s, System administrator needs to grant permissions to allow modification of this file from user space\n", ccsFile.c_str());
        }
        return;
    }

    uint32_t ccsValue = 0;
    ssize_t ret = SysCalls::read(fd, &ccsValue, sizeof(uint32_t));
    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get() && (ret < 0), stderr, "read() on %s failed errno = %d | ret = %d \n",
                       ccsFile.c_str(), errno, ret);

    if ((ret < 0) || (ccsValue == ccsMode)) {
        return;
    }

    do {
        ret = SysCalls::write(fd, &ccsMode, sizeof(uint32_t));
    } while (ret == -1 && errno == -EBUSY);

    if (ret > 0) {
        deviceCcsModeVec.emplace_back(ccsFile, ccsValue);
    }
};

unsigned int IoctlHelper::getIoctlRequestValueBase(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::gemClose:
        return DRM_IOCTL_GEM_CLOSE;
    case DrmIoctl::primeFdToHandle:
        return DRM_IOCTL_PRIME_FD_TO_HANDLE;
    case DrmIoctl::primeHandleToFd:
        return DRM_IOCTL_PRIME_HANDLE_TO_FD;
    case DrmIoctl::syncObjFdToHandle:
        return DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE;
    case DrmIoctl::syncObjWait:
        return DRM_IOCTL_SYNCOBJ_WAIT;
    case DrmIoctl::syncObjSignal:
        return DRM_IOCTL_SYNCOBJ_SIGNAL;
    case DrmIoctl::syncObjTimelineWait:
        return DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT;
    case DrmIoctl::syncObjTimelineSignal:
        return DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL;
    default:
        UNRECOVERABLE_IF(true);
        return 0u;
    }
}

std::string IoctlHelper::getIoctlStringBase(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::gemClose:
        return "DRM_IOCTL_GEM_CLOSE";
    case DrmIoctl::primeFdToHandle:
        return "DRM_IOCTL_PRIME_FD_TO_HANDLE";
    case DrmIoctl::primeHandleToFd:
        return "DRM_IOCTL_PRIME_HANDLE_TO_FD";
    case DrmIoctl::syncObjFdToHandle:
        return "DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE";
    case DrmIoctl::syncObjWait:
        return "DRM_IOCTL_SYNCOBJ_WAIT";
    case DrmIoctl::syncObjSignal:
        return "DRM_IOCTL_SYNCOBJ_SIGNAL";
    case DrmIoctl::syncObjTimelineWait:
        return "DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT";
    case DrmIoctl::syncObjTimelineSignal:
        return "DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL";
    default:
        UNRECOVERABLE_IF(true);
        return "";
    }
}

bool IoctlHelper::checkIfIoctlReinvokeRequired(int error, DrmIoctl ioctlRequest) const {
    return (error == EINTR || error == EAGAIN || error == EBUSY || error == -EBUSY);
}

uint64_t *IoctlHelper::getPagingFenceAddress(uint32_t vmHandleId, OsContextLinux *osContext) {
    if (osContext) {
        return osContext->getFenceAddr(vmHandleId);
    } else {
        return drm.getFenceAddr(vmHandleId);
    }
}

uint64_t IoctlHelper::acquireGpuRange(DrmMemoryManager &memoryManager, size_t &size, uint32_t rootDeviceIndex, AllocationType allocType, HeapIndex heapIndex) {
    if (heapIndex >= HeapIndex::totalHeaps) {
        return 0;
    }
    return memoryManager.acquireGpuRange(size, rootDeviceIndex, heapIndex);
}

void IoctlHelper::releaseGpuRange(DrmMemoryManager &memoryManager, void *address, size_t size, uint32_t rootDeviceIndex, AllocationType allocType) {
    memoryManager.releaseGpuRange(address, size, rootDeviceIndex);
}

void *IoctlHelper::mmapFunction(DrmMemoryManager &memoryManager, void *ptr, size_t size, int prot, int flags, int fd, off_t offset) {
    return memoryManager.mmapFunction(ptr, size, prot, flags, fd, offset);
}

int IoctlHelper::munmapFunction(DrmMemoryManager &memoryManager, void *ptr, size_t size) {
    return memoryManager.munmapFunction(ptr, size);
}

void IoctlHelper::registerMemoryToUnmap(DrmAllocation &allocation, void *pointer, size_t size, DrmAllocation::MemoryUnmapFunction unmapFunction) {
    return allocation.registerMemoryToUnmap(pointer, size, unmapFunction);
}

BufferObject *IoctlHelper::allocUserptr(DrmMemoryManager &memoryManager, const AllocationData &allocData, uintptr_t address, size_t size, uint32_t rootDeviceIndex) {
    return memoryManager.allocUserptr(address, size, allocData.type, rootDeviceIndex);
}

} // namespace NEO
