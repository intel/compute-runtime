/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/xe3_core/hw_info_xe3_core.h"

namespace NEO {

struct PTL;

template <>
struct HwMapper<IGFX_PTL> {
    enum { gfxFamily = IGFX_XE3_CORE };

    static const char *abbreviation;
    using GfxFamily = GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily;
    using GfxProduct = PTL;
};
} // namespace NEO
