/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

namespace NEO {

unsigned int IoctlHelperXe::getIoctlRequestValueDebugger(DrmIoctl ioctlRequest) const {
    switch (ioctlRequest) {
    case DrmIoctl::debuggerOpen:
        return euDebugInterface->getParamValue(EuDebugParam::connect);
    case DrmIoctl::metadataCreate:
        return euDebugInterface->getParamValue(EuDebugParam::metadataCreate);
    case DrmIoctl::metadataDestroy:
        return euDebugInterface->getParamValue(EuDebugParam::metadataDestroy);

    default:
        UNRECOVERABLE_IF(true);
        return 0;
    }
}

int IoctlHelperXe::debuggerOpenIoctl(DrmIoctl request, void *arg) {
    EuDebugConnect *connect = static_cast<EuDebugConnect *>(arg);
    auto ret = IoctlHelper::ioctl(request, arg);
    xeLog(" -> IoctlHelperXe::ioctl debuggerOpen pid=%llu r=%d\n",
          connect->pid, ret);
    return ret;
}

int IoctlHelperXe::debuggerMetadataCreateIoctl(DrmIoctl request, void *arg) {
    DebugMetadataCreate *metadata = static_cast<DebugMetadataCreate *>(arg);
    auto ret = IoctlHelper::ioctl(request, arg);
    xeLog(" -> IoctlHelperXe::ioctl metadataCreate type=%llu user_addr=%uul len=%uul\n id=%uul ret=%d, errno=%d\n",
          metadata->type, metadata->userAddr, metadata->len, metadata->metadataId, ret, errno);
    return ret;
}

int IoctlHelperXe::debuggerMetadataDestroyIoctl(DrmIoctl request, void *arg) {
    DebugMetadataDestroy *metadata = static_cast<DebugMetadataDestroy *>(arg);
    auto ret = IoctlHelper::ioctl(request, arg);
    xeLog(" -> IoctlHelperXe::ioctl metadataDestroy id=%llu r=%d\n",
          metadata->metadataId, ret);
    return ret;
}

int IoctlHelperXe::getEudebugExtProperty() {
    return euDebugInterface->getParamValue(EuDebugParam::execQueueSetPropertyEuDebug);
}

uint64_t IoctlHelperXe::getEudebugExtPropertyValue() {
    uint64_t val = euDebugInterface->getParamValue(EuDebugParam::execQueueSetPropertyValueEnable);
    if (euDebugInterface->isExecQueuePageFaultEnableSupported()) {
        val |= euDebugInterface->getParamValue(EuDebugParam::execQueueSetPropertyValuePageFaultEnable);
    }
    return val;
}

int IoctlHelperXe::getEuDebugSysFsEnable() {
    return euDebugInterface != nullptr ? 1 : 0;
}

uint32_t IoctlHelperXe::registerResource(DrmResourceClass classType, const void *data, size_t size) {
    DebugMetadataCreate metadata = {};
    if (classType == DrmResourceClass::elf) {
        metadata.type = euDebugInterface->getParamValue(EuDebugParam::metadataElfBinary);
        metadata.userAddr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else if (classType == DrmResourceClass::l0ZebinModule) {
        metadata.type = euDebugInterface->getParamValue(EuDebugParam::metadataProgramModule);
        metadata.userAddr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else if (classType == DrmResourceClass::contextSaveArea) {
        metadata.type = euDebugInterface->getParamValue(EuDebugParam::metadataSipArea);
        metadata.userAddr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else if (classType == DrmResourceClass::sbaTrackingBuffer) {
        metadata.type = euDebugInterface->getParamValue(EuDebugParam::metadataSbaArea);
        metadata.userAddr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else if (classType == DrmResourceClass::moduleHeapDebugArea) {
        metadata.type = euDebugInterface->getParamValue(EuDebugParam::metadataModuleArea);
        metadata.userAddr = reinterpret_cast<uintptr_t>(data);
        metadata.len = size;
    } else {
        UNRECOVERABLE_IF(true);
    }
    [[maybe_unused]] auto retVal = IoctlHelperXe::ioctl(DrmIoctl::metadataCreate, &metadata);
    PRINT_DEBUGGER_INFO_LOG("DRM_XE_DEBUG_METADATA_CREATE: type=%llu user_addr=%llu len=%llu id=%llu\n",
                            metadata.type, metadata.userAddr, metadata.len, metadata.metadataId);
    DEBUG_BREAK_IF(retVal != 0);
    return metadata.metadataId;
}

void IoctlHelperXe::unregisterResource(uint32_t handle) {
    DebugMetadataDestroy metadata = {};
    metadata.metadataId = handle;
    [[maybe_unused]] auto retVal = IoctlHelperXe::ioctl(DrmIoctl::metadataDestroy, &metadata);
    DEBUG_BREAK_IF(retVal != 0);
    PRINT_DEBUGGER_INFO_LOG("DRM_XE_DEBUG_METADATA_DESTROY: id=%llu\n", metadata.metadataId);
}

std::unique_ptr<uint8_t[]> IoctlHelperXe::prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles, uint64_t cookie) {

    static_assert(std::is_trivially_destructible_v<VmBindOpExtAttachDebug>,
                  "Storage must be allowed to be reused without calling the destructor!");

    static_assert(alignof(VmBindOpExtAttachDebug) <= __STDCPP_DEFAULT_NEW_ALIGNMENT__,
                  "Alignment of a buffer returned via new[] operator must allow storing the required type!");

    xeLog(" -> IoctlHelperXe::%s\n", __FUNCTION__);
    const auto bufferSize{sizeof(VmBindOpExtAttachDebug) * bindExtHandles.size()};
    std::unique_ptr<uint8_t[]> extensionsBuffer{new uint8_t[bufferSize]};

    auto extensions = new (extensionsBuffer.get()) VmBindOpExtAttachDebug[bindExtHandles.size()];
    std::memset(extensionsBuffer.get(), 0, bufferSize);

    extensions[0].metadataId = bindExtHandles[0];
    extensions[0].base.name = euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsAttachDebug);
    extensions[0].cookie = cookie;

    for (size_t i = 1; i < bindExtHandles.size(); i++) {
        extensions[i - 1].base.nextExtension = reinterpret_cast<uint64_t>(&extensions[i]);
        extensions[i].metadataId = bindExtHandles[i];
        extensions[i].base.name = euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsAttachDebug);
        extensions[i].cookie = cookie;
    }
    return extensionsBuffer;
}

} // namespace NEO
