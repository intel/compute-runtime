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
    if (debugManager.flags.DisableForceToStateless.get()) {
        return false;
    }
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isSubgroupNamedBarrierSupported() const {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool CompilerProductHelperHw<gfxProduct>::isSubgroupExtendedBlockReadSupported() const {
    return true;
}

} // namespace NEO
