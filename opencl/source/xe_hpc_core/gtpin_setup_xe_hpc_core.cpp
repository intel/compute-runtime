/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/gtpin/gtpin_hw_helper.h"
#include "opencl/source/gtpin/gtpin_hw_helper.inl"
#include "opencl/source/gtpin/gtpin_hw_helper_xehp_and_later.inl"

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

extern GTPinHwHelper *gtpinHwHelperFactory[IGFX_MAX_CORE];

using Family = XE_HPC_COREFamily;
static const auto gfxFamily = IGFX_XE_HPC_CORE;

template class GTPinHwHelperHw<Family>;

struct GTPinEnableXeHpcCore {
    GTPinEnableXeHpcCore() {
        gtpinHwHelperFactory[gfxFamily] = &GTPinHwHelperHw<Family>::get();
    }
};

#include "gtpin_setup_xe_hpc_core.inl"

static GTPinEnableXeHpcCore gtpinEnable;

} // namespace NEO
