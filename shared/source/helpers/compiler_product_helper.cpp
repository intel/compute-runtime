/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

namespace NEO {

CompilerProductHelperCreateFunctionType compilerProductHelperFactory[IGFX_MAX_PRODUCT] = {};

uint32_t CompilerProductHelper::getHwIpVersion(const HardwareInfo &hwInfo) const {
    if (debugManager.flags.OverrideHwIpVersion.get() != -1) {
        return debugManager.flags.OverrideHwIpVersion.get();
    }
    return getProductConfigFromHwInfo(hwInfo);
}

} // namespace NEO
