/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/local_memory_helper.h"

namespace NEO {
extern LocalMemoryHelper *localMemoryHelperFactory[IGFX_MAX_PRODUCT];

struct EnableProductLocalMemoryHelperDg1 {
    EnableProductLocalMemoryHelperDg1() {
        LocalMemoryHelper *plocalMemHelper = LocalMemoryHelperImpl<IGFX_DG1>::get();
        localMemoryHelperFactory[IGFX_DG1] = plocalMemHelper;
    }
};

static EnableProductLocalMemoryHelperDg1 enableLocalMemoryHelperDg1;
} // namespace NEO
