/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/xe_hpg_core/hw_info_xe_hpg_core.h"

namespace NEO {

struct DG2;

template <>
struct HwMapper<IGFX_DG2> {
    enum { gfxFamily = IGFX_XE_HPG_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef DG2 GfxProduct;
};
} // namespace NEO