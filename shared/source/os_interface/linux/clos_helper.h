/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/cache_info.h"

namespace NEO {
namespace ClosHelper {
/*
    PAT Index  CLOS   MemType
    SHARED
    0          0      UC (00)
    1          0      WC (01)
    2          0      WT (10)
    3          0      WB (11)
    RESERVED 1
    4          1      WT (10)
    5          1      WB (11)
    RESERVED 2
    6          2      WT (10)
    7          2      WB (11)
 */

static inline uint64_t getPatIndex(CacheRegion closIndex, CachePolicy memType) {
    if ((DebugManager.flags.ForceAllResourcesUncached.get() == true)) {
        closIndex = CacheRegion::Default;
        memType = CachePolicy::Uncached;
    }

    UNRECOVERABLE_IF((closIndex > CacheRegion::Default) && (memType < CachePolicy::WriteThrough));
    return (static_cast<uint32_t>(memType) + (static_cast<uint16_t>(closIndex) * 2));
}
} // namespace ClosHelper

} // namespace NEO
