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

} // namespace NEO
