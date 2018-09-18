/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"
#include "runtime/gtpin/gtpin_hw_helper.h"
#include "runtime/gtpin/gtpin_hw_helper.inl"

namespace OCLRT {

extern GTPinHwHelper *gtpinHwHelperFactory[IGFX_MAX_CORE];

typedef SKLFamily Family;
static const auto gfxFamily = IGFX_GEN9_CORE;

template <>
uint32_t GTPinHwHelperHw<Family>::getGenVersion() {
    return gtpin::GTPIN_GEN_9;
}

template class GTPinHwHelperHw<Family>;

struct GTPinEnableGen9 {
    GTPinEnableGen9() {
        gtpinHwHelperFactory[gfxFamily] = &GTPinHwHelperHw<Family>::get();
    }
};

static GTPinEnableGen9 gtpinEnable;

} // namespace OCLRT
