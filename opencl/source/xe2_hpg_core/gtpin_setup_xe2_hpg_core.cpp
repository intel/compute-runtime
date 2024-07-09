/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"

#include "opencl/source/gtpin/gtpin_gfx_core_helper.h"
#include "opencl/source/gtpin/gtpin_gfx_core_helper.inl"
#include "opencl/source/gtpin/gtpin_gfx_core_helper_xehp_and_later.inl"

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

extern GTPinGfxCoreHelperCreateFunctionType gtpinGfxCoreHelperFactory[IGFX_MAX_CORE];

using Family = Xe2HpgCoreFamily;
static const auto gfxFamily = IGFX_XE2_HPG_CORE;

template <>
uint32_t GTPinGfxCoreHelperHw<Family>::getGenVersion() const {
    return gtpin::GTPIN_XE_HPG_CORE;
}

template class GTPinGfxCoreHelperHw<Family>;

struct GTPinEnableXe2HpgCore {
    GTPinEnableXe2HpgCore() {
        gtpinGfxCoreHelperFactory[gfxFamily] = GTPinGfxCoreHelperHw<Family>::create;
    }
};

static GTPinEnableXe2HpgCore gtpinEnable;

} // namespace NEO
