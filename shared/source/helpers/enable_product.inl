/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/enable_product.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/hw_mapper.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
EnableGfxProductHw<gfxProduct>::EnableGfxProductHw() {
    using GfxProduct = typename HwMapper<gfxProduct>::GfxProduct;
    EnableGfxFamilyHw<ToGfxCoreFamily<gfxProduct>::get()> enableFamily;

    hardwarePrefix[gfxProduct] = HwMapper<gfxProduct>::abbreviation;
    hardwareInfoTable[gfxProduct] = &GfxProduct::hwInfo;
    hardwareInfoSetup[gfxProduct] = GfxProduct::setupHardwareInfo;
    hardwareInfoBaseSetup[gfxProduct] = GfxProduct::setupHardwareInfoBase;
}
} // namespace NEO
