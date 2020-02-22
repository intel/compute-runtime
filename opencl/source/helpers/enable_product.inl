/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_info.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
struct EnableGfxProductHw {
    typedef typename HwMapper<gfxProduct>::GfxProduct GfxProduct;
    enum { gfxFamily = HwMapper<gfxProduct>::gfxFamily };

    EnableGfxProductHw() {
        EnableGfxFamilyHw<static_cast<GFXCORE_FAMILY>(gfxFamily)> enableFamily;

        hardwarePrefix[gfxProduct] = HwMapper<gfxProduct>::abbreviation;
        defaultHardwareInfoConfigTable[gfxProduct] = GfxProduct::defaultHardwareInfoConfig;
        hardwareInfoTable[gfxProduct] = &GfxProduct::hwInfo;
        hardwareInfoSetup[gfxProduct] = GfxProduct::setupHardwareInfo;
    }
};
} // namespace NEO
