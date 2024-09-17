/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

#include "debug_xe_includes.h"

#include <fstream>
#include <sstream>

#define STRINGIFY_ME(X) return #X
#define RETURN_ME(X) return X

namespace NEO {

unsigned int IoctlHelperXe::getIoctlRequestValueDebugger(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::debuggerOpen:
        RETURN_ME(DRM_IOCTL_XE_EUDEBUG_CONNECT);
    case DrmIoctl::metadataCreate:
        RETURN_ME(DRM_IOCTL_XE_DEBUG_METADATA_CREATE);
    case DrmIoctl::metadataDestroy:
        RETURN_ME(DRM_IOCTL_XE_DEBUG_METADATA_DESTROY);

    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

int IoctlHelperXe::debuggerOpenIoctl(DrmIoctl request, void *arg) {
    drm_xe_eudebug_connect *connect = static_cast<drm_xe_eudebug_connect *>(arg);
    auto ret = IoctlHelper::ioctl(request, arg);
    xeLog(" -> IoctlHelperXe::ioctl debuggerOpen pid=%llu r=%d\n",
          connect->pid, ret);
    return ret;
}

int IoctlHelperXe::debuggerMetadataCreateIoctl(DrmIoctl request, void *arg) {
    drm_xe_debug_metadata_create *metadata = static_cast<drm_xe_debug_metadata_create *>(arg);
    auto ret = IoctlHelper::ioctl(request, arg);
    xeLog(" -> IoctlHelperXe::ioctl metadataCreate type=%llu user_addr=%uul len=%uul\n id=%uul ret=%d, errno=%d\n",
          metadata->type, metadata->user_addr, metadata->len, metadata->metadata_id, ret, errno);
    return ret;
}

int IoctlHelperXe::debuggerMetadataDestroyIoctl(DrmIoctl request, void *arg) {
    drm_xe_debug_metadata_destroy *metadata = static_cast<drm_xe_debug_metadata_destroy *>(arg);
    auto ret = IoctlHelper::ioctl(request, arg);
    xeLog(" -> IoctlHelperXe::ioctl metadataDestroy id=%llu r=%d\n",
          metadata->metadata_id, ret);
    return ret;
}

int IoctlHelperXe::getEudebugExtProperty() {
    return DRM_XE_EXEC_QUEUE_SET_PROPERTY_EUDEBUG;
}

int IoctlHelperXe::getEuDebugSysFsEnable() {
    char enabledEuDebug = '0';
    std::string sysFsPciPath = drm.getSysFsPciPath();
    std::string euDebugPath = sysFsPciPath + sysFsXeEuDebugFile;

    FILE *fileDescriptor = IoFunctions::fopenPtr(euDebugPath.c_str(), "r");
    if (fileDescriptor) {
        [[maybe_unused]] auto bytesRead = IoFunctions::freadPtr(&enabledEuDebug, 1, 1, fileDescriptor);
        IoFunctions::fclosePtr(fileDescriptor);
    }

    return enabledEuDebug - '0';
}

uint32_t IoctlHelperXe::registerResource(DrmResourceClass classType, const void *data, size_t size) {
    drm_xe_debug_metadata_create metadata = {};
    if (classType == DrmResourceClass::elf) {
        metadata.type = DRM_XE_DEBUG_METADATA_ELF_BINARY;
        metadata.user_addr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else if (classType == DrmResourceClass::l0ZebinModule) {
        metadata.type = DRM_XE_DEBUG_METADATA_PROGRAM_MODULE;
        metadata.user_addr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else if (classType == DrmResourceClass::contextSaveArea) {
        metadata.type = WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SIP_AREA;
        metadata.user_addr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else if (classType == DrmResourceClass::sbaTrackingBuffer) {
        metadata.type = WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_SBA_AREA;
        metadata.user_addr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else if (classType == DrmResourceClass::moduleHeapDebugArea) {
        metadata.type = WORK_IN_PROGRESS_DRM_XE_DEBUG_METADATA_MODULE_AREA;
        metadata.user_addr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else {
        UNRECOVERABLE_IF(true);
    }
    [[maybe_unused]] auto retVal = IoctlHelperXe::ioctl(DrmIoctl::metadataCreate, &metadata);
    PRINT_DEBUGGER_INFO_LOG("DRM_XE_DEBUG_METADATA_CREATE: type=%llu user_addr=%llu len=%llu id=%llu\n",
                            metadata.type, metadata.user_addr, metadata.len, metadata.metadata_id);
    DEBUG_BREAK_IF(retVal != 0);
    return metadata.metadata_id;
}

void IoctlHelperXe::unregisterResource(uint32_t handle) {
    drm_xe_debug_metadata_destroy metadata = {};
    metadata.metadata_id = handle;
    [[maybe_unused]] auto retVal = IoctlHelperXe::ioctl(DrmIoctl::metadataDestroy, &metadata);
    DEBUG_BREAK_IF(retVal != 0);
    PRINT_DEBUGGER_INFO_LOG("DRM_XE_DEBUG_METADATA_DESTROY: id=%llu\n", metadata.metadata_id);
}

std::unique_ptr<uint8_t[]> IoctlHelperXe::prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles) {

    static_assert(std::is_trivially_destructible_v<drm_xe_vm_bind_op_ext_attach_debug>,
                  "Storage must be allowed to be reused without calling the destructor!");

    static_assert(alignof(drm_xe_vm_bind_op_ext_attach_debug) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                  "Alignment of a buffer returned via new[] operator must allow storing the required type!");

    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    const auto bufferSize{sizeof(drm_xe_vm_bind_op_ext_attach_debug) * bindExtHandles.size()};
    std::unique_ptr<uint8_t[]> extensionsBuffer{new uint8_t[bufferSize]};

    auto extensions = new (extensionsBuffer.get()) drm_xe_vm_bind_op_ext_attach_debug[bindExtHandles.size()];
    std::memset(extensionsBuffer.get(), 0, bufferSize);

    extensions[0].metadata_id = bindExtHandles[0];
    extensions[0].base.name = XE_VM_BIND_OP_EXTENSIONS_ATTACH_DEBUG;

    for (size_t i = 1; i < bindExtHandles.size(); i++) {
        extensions[i - 1].base.next_extension = reinterpret_cast<uint64_t>(&extensions[i]);
        extensions[i].metadata_id = bindExtHandles[i];
        extensions[i].base.name = XE_VM_BIND_OP_EXTENSIONS_ATTACH_DEBUG;
    }
    return extensionsBuffer;
}

} // namespace NEO