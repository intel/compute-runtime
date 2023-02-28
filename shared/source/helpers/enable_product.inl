/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/hw_mapper.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
struct EnableGfxProductHw {
    typedef typename HwMapper<gfxProduct>::GfxProduct GfxProduct;
    enum { gfxFamily = HwMapper<gfxProduct>::gfxFamily };

    EnableGfxProductHw() {
        EnableGfxFamilyHw<static_cast<GFXCORE_FAMILY>(gfxFamily)> enableFamily;

        hardwarePrefix[gfxProduct] = HwMapper<gfxProduct>::abbreviation;
        hardwareInfoTable[gfxProduct] = &GfxProduct::hwInfo;
        hardwareInfoSetup[gfxProduct] = GfxProduct::setupHardwareInfo;
        hardwareInfoBaseSetup[gfxProduct] = GfxProduct::setupHardwareInfoBase;
    }
};
} // namespace NEO
