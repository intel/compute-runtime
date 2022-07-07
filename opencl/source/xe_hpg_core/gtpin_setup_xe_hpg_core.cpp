/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/gtpin/gtpin_hw_helper.h"
#include "opencl/source/gtpin/gtpin_hw_helper.inl"
#include "opencl/source/gtpin/gtpin_hw_helper_xehp_and_later.inl"

#include "hw_cmds_xe_hpg_core_base.h"
#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

extern GTPinHwHelper *gtpinHwHelperFactory[IGFX_MAX_CORE];

typedef XE_HPG_COREFamily Family;
static const auto gfxFamily = IGFX_XE_HPG_CORE;

template class GTPinHwHelperHw<Family>;

struct GTPinEnableXeHpgCore {
    GTPinEnableXeHpgCore() {
        gtpinHwHelperFactory[gfxFamily] = &GTPinHwHelperHw<Family>::get();
    }
};

template <>
uint32_t GTPinHwHelperHw<Family>::getGenVersion() {
    return gtpin::GTPIN_XE_HPG_CORE;
}

static GTPinEnableXeHpgCore gtpinEnable;

} // namespace NEO
