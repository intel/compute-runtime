/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_info_xe2_hpg_core.h"

namespace NEO {

struct BMG;

template <>
struct HwMapper<IGFX_BMG> {
    enum { gfxFamily = IGFX_XE2_HPG_CORE };

    static const char *abbreviation;
    using GfxFamily = GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily;
    using GfxProduct = BMG;
};
} // namespace NEO
