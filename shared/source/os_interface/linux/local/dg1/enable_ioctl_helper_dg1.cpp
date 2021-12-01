/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

namespace NEO {
extern IoctlHelper *ioctlHelperFactory[IGFX_MAX_PRODUCT];

struct EnableProductIoctlHelperDg1 {
    EnableProductIoctlHelperDg1() {
        IoctlHelper *pIoctlHelper = IoctlHelperImpl<IGFX_DG1>::get();
        ioctlHelperFactory[IGFX_DG1] = pIoctlHelper;
    }
};

static EnableProductIoctlHelperDg1 enableIoctlHelperDg1;
} // namespace NEO
