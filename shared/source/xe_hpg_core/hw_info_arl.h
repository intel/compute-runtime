/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/xe_hpg_core/hw_info_xe_hpg_core.h"

namespace NEO {

struct ARL;

template <>
struct HwMapper<IGFX_ARROWLAKE> {
    enum { gfxFamily = IGFX_XE_HPG_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef ARL GfxProduct;
};
} // namespace NEO
