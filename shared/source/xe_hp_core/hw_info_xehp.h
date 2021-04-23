/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_info_xe_hp_core.h"

namespace NEO {

struct XEHP;

template <>
struct HwMapper<IGFX_XE_HP_SDV> {
    enum { gfxFamily = IGFX_XE_HP_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef XEHP GfxProduct;
};
} // namespace NEO
