/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

namespace NEO {
extern IoctlHelper *ioctlHelperFactory[IGFX_MAX_PRODUCT];

struct EnableProductIoctlHelperXeHpSdv {
    EnableProductIoctlHelperXeHpSdv() {
        IoctlHelper *pIoctlHelper = IoctlHelperImpl<IGFX_XE_HP_SDV>::get();
        ioctlHelperFactory[IGFX_XE_HP_SDV] = pIoctlHelper;
    }
};

static EnableProductIoctlHelperXeHpSdv enableIoctlHelperXeHpSdv;
} // namespace NEO
