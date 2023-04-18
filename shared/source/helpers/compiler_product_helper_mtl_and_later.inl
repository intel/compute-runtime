/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
uint32_t CompilerProductHelperHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return hwInfo.ipVersion.value;
}
template <PRODUCT_FAMILY gfxProduct>
void CompilerProductHelperHw<gfxProduct>::setProductConfigForHwInfo(HardwareInfo &hwInfo, HardwareIpVersion config) const {
    hwInfo.ipVersion = config;
}
} // namespace NEO
