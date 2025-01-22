/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe_prelim.h"

namespace NEO {

void IoctlHelperXePrelim::setContextProperties(const OsContextLinux &osContext, void *extProperties, uint32_t &extIndexInOut) {
    IoctlHelperXe::setContextProperties(osContext, extProperties, extIndexInOut);
}

bool IoctlHelperXePrelim::getFdFromVmExport(uint32_t vmId, uint32_t flags, int32_t *fd) {
    return IoctlHelperXe::getFdFromVmExport(vmId, flags, fd);
}

unsigned int IoctlHelperXePrelim::getIoctlRequestValue(DrmIoctl ioctlRequest) const {
    return IoctlHelperXe::getIoctlRequestValue(ioctlRequest);
}

std::string IoctlHelperXePrelim::getIoctlString(DrmIoctl ioctlRequest) const {
    return IoctlHelperXe::getIoctlString(ioctlRequest);
}

} // namespace NEO
