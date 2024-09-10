/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe_prelim.h"

#include "shared/source/os_interface/linux/xe/xedrm_prelim.h"

namespace NEO {

IoctlHelperXePrelim::~IoctlHelperXePrelim() {
    xeLog("IoctlHelperXePrelim::~IoctlHelperXePrelim\n", "");
}
} // namespace NEO
