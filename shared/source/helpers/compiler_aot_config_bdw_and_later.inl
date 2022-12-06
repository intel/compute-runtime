/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {
template <PRODUCT_FAMILY gfxProduct>
void CompilerProductHelperHw<gfxProduct>::setProductConfigForHwInfo(HardwareInfo &hwInfo, HardwareIpVersion config) const {
    hwInfo.platform.usRevId = config.revision;
}

} // namespace NEO
