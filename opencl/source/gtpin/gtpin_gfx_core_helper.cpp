/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/gtpin/gtpin_gfx_core_helper.h"

namespace NEO {
GTPinGfxCoreHelperCreateFunctionType gtpinGfxCoreHelperFactory[IGFX_MAX_CORE] = {};

std::unique_ptr<GTPinGfxCoreHelper> GTPinGfxCoreHelper::create(GFXCORE_FAMILY gfxCore) {
    auto createFunction = gtpinGfxCoreHelperFactory[gfxCore];
    if (createFunction == nullptr) {
        return nullptr;
    }
    auto gtpinGfxCoreHelper = createFunction();
    return gtpinGfxCoreHelper;
}

} // namespace NEO
