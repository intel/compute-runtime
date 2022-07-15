/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/compiler_hw_info_config.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
bool CompilerHwInfoConfigHw<gfxProduct>::isForceToStatelessRequired() const {
    if (DebugManager.flags.DisableForceToStateless.get()) {
        return false;
    }
    return true;
}

} // namespace NEO
