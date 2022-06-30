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

typedef XeHpFamily Family;
static const auto gfxFamily = IGFX_XE_HP_CORE;

template <>
uint32_t GTPinHwHelperHw<Family>::getGenVersion() {
    return gtpin::GTPIN_XEHP_CORE;
}

template class GTPinHwHelperHw<Family>;

struct GTPinEnableXeHpCore {
    GTPinEnableXeHpCore() {
        gtpinHwHelperFactory[gfxFamily] = &GTPinHwHelperHw<Family>::get();
    }
};

static GTPinEnableXeHpCore gtpinEnable;

} // namespace NEO
