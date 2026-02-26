/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

namespace NEO {

unsigned int IoctlHelperXe::getIoctlRequestValueDebugger(DrmIoctl ioctlRequest) const {
    UNRECOVERABLE_IF(true);
    return 0;
}

int IoctlHelperXe::debuggerOpenIoctl(DrmIoctl request, void *arg) {
    UNRECOVERABLE_IF(true);
    return 0;
}

int IoctlHelperXe::debuggerMetadataCreateIoctl(DrmIoctl request, void *arg) {
    UNRECOVERABLE_IF(true);
    return 0;
}

int IoctlHelperXe::debuggerMetadataDestroyIoctl(DrmIoctl request, void *arg) {
    UNRECOVERABLE_IF(true);
    return 0;
}

int IoctlHelperXe::getEudebugExtProperty() {
    UNRECOVERABLE_IF(true);
    return 0;
}

uint64_t IoctlHelperXe::getEudebugExtPropertyValue() {
    UNRECOVERABLE_IF(true);
    return 0;
}

int IoctlHelperXe::getEuDebugSysFsEnable() {
    return 0;
}

uint32_t IoctlHelperXe::registerResource(DrmResourceClass classType, const void *data, size_t size) {
    UNRECOVERABLE_IF(true);
    return 0;
}

void IoctlHelperXe::unregisterResource(uint32_t handle) {
    UNRECOVERABLE_IF(true);
}

std::unique_ptr<uint8_t[]> IoctlHelperXe::prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles, uint64_t cookie) {
    return {};
}

std::optional<std::vector<VmBindOpExtDebugData>> IoctlHelperXe::addDebugDataAndCreateBindOpVec(BufferObject *bo, uint32_t vmId, bool isAdd) {
    return std::nullopt;
}

int IoctlHelperXe::bindAddDebugData(std::vector<VmBindOpExtDebugData> debugDataVec, uint32_t vmHandleId, VmBindExtUserFenceT *vmBindExtUserFence, bool isAdd) {
    UNRECOVERABLE_IF(true);
}

uint64_t IoctlHelperXe::convertDrmResourceClassToXeDebugPseudoPath(DrmResourceClass resourceClass) {
    UNRECOVERABLE_IF(true);
}

} // namespace NEO
