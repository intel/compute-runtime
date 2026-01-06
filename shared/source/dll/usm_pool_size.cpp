/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/usm_pool_params.h"

namespace NEO {
size_t UsmPoolParams::getUsmPoolSize(const GfxCoreHelper &gfxCoreHelper) {
    return gfxCoreHelper.isExtendedUsmPoolSizeEnabled() ? 32 * MemoryConstants::megaByte : 2 * MemoryConstants::megaByte;
}
} // namespace NEO
