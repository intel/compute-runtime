/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_cache.h"
#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/kernel/implicit_args.h"

#include "opencl/source/gtpin/gtpin_hw_helper.h"
#include "opencl/source/gtpin/gtpin_hw_helper.inl"
#include "opencl/source/gtpin/gtpin_hw_helper_xehp_and_later.inl"

#include "hw_cmds_xe_hpg_core_base.h"
#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

extern GTPinGfxCoreHelper *gtpinGfxCoreHelperFactory[IGFX_MAX_CORE];

typedef XeHpgCoreFamily Family;
static const auto gfxFamily = IGFX_XE_HPG_CORE;

template class GTPinGfxCoreHelperHw<Family>;

struct GTPinEnableXeHpgCore {
    GTPinEnableXeHpgCore() {
        gtpinGfxCoreHelperFactory[gfxFamily] = &GTPinGfxCoreHelperHw<Family>::get();
    }
};

template <>
uint32_t GTPinGfxCoreHelperHw<Family>::getGenVersion() {
    return gtpin::GTPIN_XE_HPG_CORE;
}

static GTPinEnableXeHpgCore gtpinEnable;

} // namespace NEO
