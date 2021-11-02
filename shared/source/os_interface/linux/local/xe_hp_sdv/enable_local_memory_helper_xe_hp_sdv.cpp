/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/local_memory_helper.h"

namespace NEO {
extern LocalMemoryHelper *localMemoryHelperFactory[IGFX_MAX_PRODUCT];

struct EnableProductLocalMemoryHelperXeHpSdv {
    EnableProductLocalMemoryHelperXeHpSdv() {
        LocalMemoryHelper *plocalMemHelper = LocalMemoryHelperImpl<IGFX_XE_HP_SDV>::get();
        localMemoryHelperFactory[IGFX_XE_HP_SDV] = plocalMemHelper;
    }
};

static EnableProductLocalMemoryHelperXeHpSdv enableLocalMemoryHelperXeHpSdv;
} // namespace NEO
