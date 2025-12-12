/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_info_xe3p_core.h"

namespace NEO {

struct CRI;

template <>
struct HwMapper<NEO::criProductEnumValue> {
    enum { gfxFamily = NEO::xe3pCoreEnumValue };

    static const char *abbreviation;
    using GfxFamily = GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily;
    using GfxProduct = CRI;
};
} // namespace NEO
