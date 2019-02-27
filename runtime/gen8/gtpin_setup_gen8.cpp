/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gtpin/gtpin_hw_helper.h"
#include "runtime/gtpin/gtpin_hw_helper.inl"

#include "ocl_igc_shared/gtpin/gtpin_ocl_interface.h"

namespace OCLRT {

extern GTPinHwHelper *gtpinHwHelperFactory[IGFX_MAX_CORE];

typedef BDWFamily Family;
static const auto gfxFamily = IGFX_GEN8_CORE;

template <>
uint32_t GTPinHwHelperHw<Family>::getGenVersion() {
    return gtpin::GTPIN_GEN_8;
}

template class GTPinHwHelperHw<Family>;

struct GTPinEnableGen8 {
    GTPinEnableGen8() {
        gtpinHwHelperFactory[gfxFamily] = &GTPinHwHelperHw<Family>::get();
    }
};

static GTPinEnableGen8 gtpinEnable;

} // namespace OCLRT
