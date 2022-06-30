/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <functional>

namespace NEO {
extern std::optional<std::function<std::unique_ptr<IoctlHelper>(Drm &drm)>> ioctlHelperFactory[IGFX_MAX_PRODUCT];

struct EnableProductIoctlHelperDg1 {
    EnableProductIoctlHelperDg1() {
        ioctlHelperFactory[IGFX_DG1] = IoctlHelperImpl<IGFX_DG1>::get;
    }
};

static EnableProductIoctlHelperDg1 enableIoctlHelperDg1;
} // namespace NEO
