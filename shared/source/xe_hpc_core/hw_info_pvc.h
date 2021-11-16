/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_info_xe_hpc_core.h"

namespace NEO {

struct PVC;

template <>
struct HwMapper<IGFX_PVC> {
    enum { gfxFamily = IGFX_XE_HPC_CORE };

    static const char *abbreviation;
    using GfxFamily = GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily;
    using GfxProduct = PVC;
};
} // namespace NEO
