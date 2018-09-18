/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/hw_info.h"

namespace OCLRT {

struct CNLFamily;
struct CNL;

template <>
struct GfxFamilyMapper<IGFX_GEN10_CORE> {
    typedef CNLFamily GfxFamily;
    static const char *name;
};

template <>
struct HwMapper<IGFX_CANNONLAKE> {
    enum { gfxFamily = IGFX_GEN10_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef CNL GfxProduct;
};
} // namespace OCLRT
