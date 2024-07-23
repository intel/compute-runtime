/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

namespace NEO {
void IoctlHelperXe::querySupportedFeatures(){};

uint64_t IoctlHelperXe::getAdditionalFlagsForVmBind(bool bindImmediate, bool readOnlyResource) {
    return 0;
}
} // namespace NEO