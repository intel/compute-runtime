/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/source/os_interface/linux/xe/xedrm.h"

namespace NEO {
uint32_t IoctlHelperXe::getNoVmOvercommitFlag() const {
    return DRM_XE_VM_CREATE_FLAG_NO_VM_OVERCOMMIT;
}

uint32_t IoctlHelperXe::getVmBindDecompressFlag() const {
    return DRM_XE_VM_BIND_FLAG_DECOMPRESS;
}
} // namespace NEO
