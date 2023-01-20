/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds_base.h"

#include "opencl/source/gtpin/gtpin_hw_helper.h"
#include "opencl/source/gtpin/gtpin_hw_helper.inl"
#include "opencl/source/gtpin/gtpin_hw_helper_xehp_and_later.inl"

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace NEO {

extern GTPinGfxCoreHelper *gtpinGfxCoreHelperFactory[IGFX_MAX_CORE];

typedef XeHpFamily Family;
static const auto gfxFamily = IGFX_XE_HP_CORE;

template <>
uint32_t GTPinGfxCoreHelperHw<Family>::getGenVersion() {
    return gtpin::GTPIN_XEHP_CORE;
}

template class GTPinGfxCoreHelperHw<Family>;

struct GTPinEnableXeHpCore {
    GTPinEnableXeHpCore() {
        gtpinGfxCoreHelperFactory[gfxFamily] = &GTPinGfxCoreHelperHw<Family>::get();
    }
};

static GTPinEnableXeHpCore gtpinEnable;

} // namespace NEO
