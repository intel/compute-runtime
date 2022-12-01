/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/xe_hpg_core/hw_info_xe_hpg_core.h"

namespace NEO {

struct MTL;

template <>
struct HwMapper<IGFX_METEORLAKE> {
    enum { gfxFamily = IGFX_XE_HPG_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef MTL GfxProduct;
};
} // namespace NEO
