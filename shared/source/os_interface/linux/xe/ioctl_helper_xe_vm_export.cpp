/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

namespace NEO {
bool IoctlHelperXe::getFdFromVmExport(uint32_t vmId, uint32_t flags, int32_t *fd) {
    return false;
}
} // namespace NEO
