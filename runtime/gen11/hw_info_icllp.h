/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_info_gen11.h"

namespace NEO {

struct ICLLP;

template <>
struct HwMapper<IGFX_ICELAKE_LP> {
    enum { gfxFamily = IGFX_GEN11_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef ICLLP GfxProduct;
};
} // namespace NEO
