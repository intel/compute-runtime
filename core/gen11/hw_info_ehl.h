/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/gen11/hw_info_gen11.h"

namespace NEO {

struct EHL;

template <>
struct HwMapper<IGFX_ELKHARTLAKE> {
    enum { gfxFamily = IGFX_GEN11_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef EHL GfxProduct;
};
} // namespace NEO
