/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/usm_pool_params.h"

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"

namespace NEO {
UsmPoolParams UsmPoolParams::getUsmPoolParams(const GfxCoreHelper &gfxCoreHelper) {
    return {
        .poolSize = UsmPoolParams::getUsmPoolSize(gfxCoreHelper),
        .minServicedSize = 0u,
        .maxServicedSize = gfxCoreHelper.isExtendedUsmPoolSizeEnabled() ? 2 * MemoryConstants::megaByte : MemoryConstants::megaByte};
}
} // namespace NEO
