/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gen11/hw_info_gen11.h"

namespace NEO {

struct LKF;

template <>
struct HwMapper<IGFX_LAKEFIELD> {
    enum { gfxFamily = IGFX_GEN11_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef LKF GfxProduct;
};
} // namespace NEO
