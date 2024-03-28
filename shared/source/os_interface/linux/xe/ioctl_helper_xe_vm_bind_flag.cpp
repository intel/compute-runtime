/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

namespace NEO {

uint64_t IoctlHelperXe::getExtraFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident, bool bindLock, bool readOnlyResource) {
    return 0;
}

} // namespace NEO
