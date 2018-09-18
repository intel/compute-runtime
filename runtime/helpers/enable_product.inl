/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_info.h"

namespace OCLRT {
template <PRODUCT_FAMILY gfxProduct>
struct EnableGfxProductHw {
    typedef typename HwMapper<gfxProduct>::GfxProduct GfxProduct;
    enum { gfxFamily = HwMapper<gfxProduct>::gfxFamily };

    EnableGfxProductHw() {
        EnableGfxFamilyHw<static_cast<GFXCORE_FAMILY>(gfxFamily)> enableFamily;

        hardwarePrefix[gfxProduct] = HwMapper<gfxProduct>::abbreviation;
        hardwareInfoTable[gfxProduct] = &GfxProduct::hwInfo;
        hardwareInfoSetup[gfxProduct] = GfxProduct::setupHardwareInfo;
    }
};
} // namespace OCLRT
