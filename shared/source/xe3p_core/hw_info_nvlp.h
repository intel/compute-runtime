/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

namespace NEO {

struct NVLP;

template <>
struct HwMapper<IGFX_NVL> {
    enum { gfxFamily = IGFX_XE3P_CORE };

    static const char *abbreviation;
    using GfxFamily = GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily;
    using GfxProduct = NVLP;
};
} // namespace NEO
