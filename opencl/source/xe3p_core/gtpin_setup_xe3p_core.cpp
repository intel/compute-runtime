/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/xe3p_core/hw_cmds_base.h"

#include "opencl/source/gtpin/gtpin_gfx_core_helper.h"
#include "opencl/source/gtpin/gtpin_gfx_core_helper.inl"
#include "opencl/source/gtpin/gtpin_gfx_core_helper_xehp_and_later.inl"

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

extern GTPinGfxCoreHelperCreateFunctionType gtpinGfxCoreHelperFactory[NEO::maxCoreEnumValue];

using Family = Xe3pCoreFamily;
static const auto gfxFamily = IGFX_XE3P_CORE;

template <>
uint32_t GTPinGfxCoreHelperHw<Family>::getGenVersion() const {
    DEBUG_BREAK_IF(true);
    return gtpin::GTPIN_XE_HPG_CORE;
}

template class GTPinGfxCoreHelperHw<Family>;

struct GTPinEnableXe3pCore {
    GTPinEnableXe3pCore() {
        gtpinGfxCoreHelperFactory[gfxFamily] = GTPinGfxCoreHelperHw<Family>::create;
    }
};

static GTPinEnableXe3pCore gtpinEnable;

} // namespace NEO
