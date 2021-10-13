/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/local_memory_helper.h"

#include "shared/source/os_interface/linux/drm_neo.h"

namespace NEO {

LocalMemoryHelper *localMemoryHelperFactory[IGFX_MAX_PRODUCT] = {};

uint32_t LocalMemoryHelper::ioctl(Drm *drm, unsigned long request, void *arg) {
    return drm->ioctl(request, arg);
}

} // namespace NEO
