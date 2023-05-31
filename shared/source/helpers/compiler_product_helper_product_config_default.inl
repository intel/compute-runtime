/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
uint32_t CompilerProductHelperHw<gfxProduct>::getProductConfigFromHwInfo(const HardwareInfo &hwInfo) const {
    return getDefaultHwIpVersion();
}
} // namespace NEO
