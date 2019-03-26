/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_info_gen9.h"

namespace NEO {

struct GLK;

template <>
struct HwMapper<IGFX_GEMINILAKE> {
    enum { gfxFamily = IGFX_GEN9_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef GLK GfxProduct;
};
} // namespace NEO
