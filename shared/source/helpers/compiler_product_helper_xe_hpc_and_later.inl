/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/compiler_product_helper.h"

namespace NEO {

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isForceToStatelessRequired() const {
    if (DebugManager.flags.DisableForceToStateless.get()) {
        return false;
    }
    return true;
}

} // namespace NEO
