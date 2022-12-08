/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/gtpin/gtpin_hw_helper.h"
#include "opencl/source/gtpin/gtpin_hw_helper.inl"
#include "opencl/source/gtpin/gtpin_hw_helper_xehp_and_later.inl"

#include "hw_cmds_xe_hpc_core_base.h"
#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

extern GTPinGfxCoreHelper *gtpinGfxCoreHelperFactory[IGFX_MAX_CORE];

using Family = XeHpcCoreFamily;
static const auto gfxFamily = IGFX_XE_HPC_CORE;

template class GTPinGfxCoreHelperHw<Family>;

struct GTPinEnableXeHpcCore {
    GTPinEnableXeHpcCore() {
        gtpinGfxCoreHelperFactory[gfxFamily] = &GTPinGfxCoreHelperHw<Family>::get();
    }
};

template <>
uint32_t GTPinGfxCoreHelperHw<Family>::getGenVersion() {
    return gtpin::GTPIN_XE_HPC_CORE;
}

static GTPinEnableXeHpcCore gtpinEnable;

} // namespace NEO
