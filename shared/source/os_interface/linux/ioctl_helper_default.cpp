/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

namespace NEO {

uint32_t IoctlHelperDefault::createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) {
    DEBUG_BREAK_IF(true);
    return -1;
}

std::unique_ptr<uint8_t[]> IoctlHelperDefault::translateIfRequired(uint8_t *dataQuery, int32_t length) {
    DEBUG_BREAK_IF(true);
    return std::unique_ptr<uint8_t[]>(dataQuery);
}

} // namespace NEO
