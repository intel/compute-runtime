/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

namespace NEO {

struct EnableProductIoctlHelperDg1 {
    EnableProductIoctlHelperDg1() {
        ioctlHelperFactory[IGFX_DG1] = IoctlHelperImpl<IGFX_DG1>::get;
    }
};

static EnableProductIoctlHelperDg1 enableIoctlHelperDg1;
} // namespace NEO
