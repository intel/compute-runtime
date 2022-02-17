/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "shared/source/os_interface/linux/drm_neo.h"

namespace NEO {

uint32_t IoctlHelper::ioctl(Drm *drm, unsigned long request, void *arg) {
    return drm->ioctl(request, arg);
}

} // namespace NEO
